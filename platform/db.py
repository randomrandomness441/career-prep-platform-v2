"""State for the prep platform. Single user, SQLite, no ORM.

The interesting part is help detection. Everything the user does is an append-only
event. Status is *derived* from the event log, never stored as a mutable flag, so
it can never drift out of sync with what actually happened:

    a hint/solution reveal, at any time BEFORE the first passing submission
    on that part, permanently marks the part as solved-with-help.

Reveals after a clean pass do not downgrade it — reading the model answer once
you've already solved it is study, not help.
"""

import json
import os
import sqlite3
import time

DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "state.db")

SCHEMA = """
CREATE TABLE IF NOT EXISTS events (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    ts      REAL NOT NULL,
    qid     TEXT NOT NULL,
    part    INTEGER NOT NULL,
    kind    TEXT NOT NULL,      -- open|run|submit_pass|submit_racy|submit_fail
                                -- |reveal_hint|reveal_solution
    payload TEXT DEFAULT ''
);
CREATE INDEX IF NOT EXISTS ix_events_q ON events(qid, part, id);

CREATE TABLE IF NOT EXISTS code (
    qid     TEXT NOT NULL,
    part    INTEGER NOT NULL,
    content TEXT NOT NULL,
    ts      REAL NOT NULL,
    PRIMARY KEY (qid, part)
);

CREATE TABLE IF NOT EXISTS stars (
    qid     TEXT PRIMARY KEY,
    note    TEXT DEFAULT ''
);

CREATE TABLE IF NOT EXISTS interviews (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    qid        TEXT NOT NULL,
    part       INTEGER NOT NULL,
    started_ts REAL NOT NULL,
    duration_s INTEGER NOT NULL,
    ended_ts   REAL,
    status     TEXT NOT NULL DEFAULT 'active',   -- active|ended
    code       TEXT DEFAULT '',
    review     TEXT DEFAULT ''
);
CREATE TABLE IF NOT EXISTS interview_messages (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    interview_id INTEGER NOT NULL,
    ts           REAL NOT NULL,
    role         TEXT NOT NULL,      -- candidate|interviewer
    text         TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS ix_interview_msgs ON interview_messages(interview_id, id);
"""


def connect():
    con = sqlite3.connect(DB_PATH)
    con.row_factory = sqlite3.Row
    con.executescript(SCHEMA)
    return con


def log(qid, part, kind, payload=""):
    con = connect()
    con.execute(
        "INSERT INTO events (ts, qid, part, kind, payload) VALUES (?,?,?,?,?)",
        (time.time(), qid, part, kind, payload if isinstance(payload, str) else json.dumps(payload)),
    )
    con.commit()
    con.close()


def save_code(qid, part, content):
    con = connect()
    con.execute(
        "INSERT INTO code (qid, part, content, ts) VALUES (?,?,?,?) "
        "ON CONFLICT(qid, part) DO UPDATE SET content=excluded.content, ts=excluded.ts",
        (qid, part, content, time.time()),
    )
    con.commit()
    con.close()


def load_code(qid, part):
    con = connect()
    row = con.execute(
        "SELECT content FROM code WHERE qid=? AND part=?", (qid, part)
    ).fetchone()
    con.close()
    return row["content"] if row else None


def star_set(qid, note):
    con = connect()
    con.execute(
        "INSERT INTO stars (qid, note) VALUES (?,?) "
        "ON CONFLICT(qid) DO UPDATE SET note=excluded.note",
        (qid, note),
    )
    con.commit()
    con.close()


def star_remove(qid):
    con = connect()
    con.execute("DELETE FROM stars WHERE qid=?", (qid,))
    con.commit()
    con.close()


def stars_all():
    """{qid: note} for every starred question."""
    con = connect()
    rows = con.execute("SELECT qid, note FROM stars").fetchall()
    con.close()
    return {r["qid"]: r["note"] for r in rows}


def interview_history(limit=50):
    """Every ended interview that got a written review, most recent first."""
    con = connect()
    rows = con.execute(
        "SELECT id, qid, part, started_ts, ended_ts, review FROM interviews "
        "WHERE status='ended' AND review != '' ORDER BY id DESC LIMIT ?",
        (limit,),
    ).fetchall()
    con.close()
    return [dict(r) for r in rows]


REVEALS = ("reveal_hint", "reveal_solution")
PASSES = ("submit_pass", "submit_racy")


def part_status(qid, part, con=None):
    """Derive a part's status by replaying its event log in order."""
    own = con is None
    if own:
        con = connect()
    rows = con.execute(
        "SELECT kind, ts, payload FROM events WHERE qid=? AND part=? ORDER BY id",
        (qid, str(part) if isinstance(part, str) else part),
    ).fetchall()
    if own:
        con.close()

    st = {
        "status": "unsolved",   # unsolved | attempted | solved
        "helped": False,        # a reveal happened before the first pass
        "racy": False,          # passed tests but TSan/stress objected
        "attempts": 0,
        "hints_used": 0,
        "saw_solution": False,
        "first_pass_ts": None,
        "opened_ts": None,
        "seconds_to_solve": None,
    }

    revealed_before_pass = False
    for r in rows:
        k = r["kind"]
        if k == "open" and st["opened_ts"] is None:
            st["opened_ts"] = r["ts"]
        elif k == "reveal_hint":
            st["hints_used"] += 1
            if st["first_pass_ts"] is None:
                revealed_before_pass = True
        elif k == "reveal_solution":
            st["saw_solution"] = True
            if st["first_pass_ts"] is None:
                revealed_before_pass = True
        elif k in ("submit_pass", "submit_racy", "submit_fail"):
            st["attempts"] += 1
            if k in PASSES and st["first_pass_ts"] is None:
                st["first_pass_ts"] = r["ts"]
                st["status"] = "solved"
                st["helped"] = revealed_before_pass
                st["racy"] = k == "submit_racy"
                if st["opened_ts"]:
                    st["seconds_to_solve"] = int(r["ts"] - st["opened_ts"])
            elif k == "submit_racy" and st["status"] == "solved" and st["racy"]:
                pass
            elif k == "submit_pass" and st["racy"]:
                st["racy"] = False        # they came back and fixed the race
            if st["status"] == "unsolved":
                st["status"] = "attempted"

    return st


def label(st):
    """Short human label used by the list view."""
    if st["status"] == "unsolved":
        return "unsolved"
    if st["status"] == "attempted":
        return "attempted"
    if st["racy"]:
        return "solved (racy)"
    return "solved (with help)" if st["helped"] else "solved"


def all_status(questions):
    """questions: list of dicts with id + parts count. Returns {qid: {...}}."""
    con = connect()
    out = {}
    for q in questions:
        parts = {}
        for p in range(q["parts"]):
            parts[p] = part_status(q["id"], p, con)
        solved = sum(1 for s in parts.values() if s["status"] == "solved")
        out[q["id"]] = {
            "parts": parts,
            "solved_parts": solved,
            "total_parts": q["parts"],
            "base": parts.get(0, {}),
            "complete": solved == q["parts"],
        }
    con.close()
    return out


def revision_queue(questions):
    """Everything that needs another look: helped, racy, or attempted-not-solved."""
    st = all_status(questions)
    out = []
    for q in questions:
        for p, s in st[q["id"]]["parts"].items():
            if s["status"] == "attempted" or (s["status"] == "solved" and (s["helped"] or s["racy"])):
                out.append({
                    "qid": q["id"], "title": q["title"], "part": p,
                    "label": label(s), "hints_used": s["hints_used"],
                    "saw_solution": s["saw_solution"], "attempts": s["attempts"],
                })
    out.sort(key=lambda r: (r["label"] != "attempted", r["qid"]))
    return out


def pack_rollup(questions):
    """Progress grouped by topic folder instead of flattened across all of them --
    lets a fork with more than one questions/<topic>/ pack (its own alongside a
    borrowed one) see "3/12 done in spark-tuning, 40/68 in concurrency" instead of
    one combined number that means nothing once more than one skill is in play."""
    st = all_status(questions)
    out = {}
    for q in questions:
        r = out.setdefault(q["topic"], {"solved": 0, "total": 0, "helped": 0, "racy": 0})
        for p in st[q["id"]]["parts"].values():
            r["total"] += 1
            if p["status"] == "solved":
                r["solved"] += 1
                if p["helped"]:
                    r["helped"] += 1
                if p["racy"]:
                    r["racy"] += 1
    return out


# ── mock interview ───────────────────────────────────────────────────────
# A timed session lives entirely in its own two tables and never touches
# `code`, `notes`, or `events` -- it must not affect a question's real saved
# practice code or its solved/helped/racy status.

def interview_active():
    con = connect()
    row = con.execute(
        "SELECT * FROM interviews WHERE status='active' ORDER BY id DESC LIMIT 1"
    ).fetchone()
    con.close()
    return dict(row) if row else None


def interview_last():
    """Most recent interview regardless of status -- lets a client resume a
    just-ended one (to fetch its review) without already knowing its id."""
    con = connect()
    row = con.execute("SELECT * FROM interviews ORDER BY id DESC LIMIT 1").fetchone()
    con.close()
    return dict(row) if row else None


def interview_get(interview_id):
    con = connect()
    row = con.execute("SELECT * FROM interviews WHERE id=?", (interview_id,)).fetchone()
    con.close()
    return dict(row) if row else None


def interview_start(qid, part, duration_s):
    """Idempotent: returns the already-running interview if one exists,
    instead of starting a second one (single-user, single-session app)."""
    active = interview_active()
    if active:
        return active
    con = connect()
    cur = con.execute(
        "INSERT INTO interviews (qid, part, started_ts, duration_s, status, code, review) "
        "VALUES (?,?,?,?, 'active', '', '')",
        (qid, part, time.time(), duration_s),
    )
    con.commit()
    iid = cur.lastrowid
    con.close()
    return interview_get(iid)


def interview_messages_since(interview_id, since_id=0):
    con = connect()
    rows = con.execute(
        "SELECT id, ts, role, text FROM interview_messages "
        "WHERE interview_id=? AND id>? ORDER BY id",
        (interview_id, since_id),
    ).fetchall()
    con.close()
    return [dict(r) for r in rows]


def interview_add_message(interview_id, role, text):
    con = connect()
    con.execute(
        "INSERT INTO interview_messages (interview_id, ts, role, text) VALUES (?,?,?,?)",
        (interview_id, time.time(), role, text),
    )
    con.commit()
    con.close()


def interview_save_code(interview_id, code):
    con = connect()
    con.execute("UPDATE interviews SET code=? WHERE id=?", (code, interview_id))
    con.commit()
    con.close()


def interview_end(interview_id):
    con = connect()
    con.execute(
        "UPDATE interviews SET status='ended', ended_ts=? WHERE id=? AND status!='ended'",
        (time.time(), interview_id),
    )
    con.commit()
    con.close()


def interview_set_review(interview_id, review):
    con = connect()
    con.execute("UPDATE interviews SET review=? WHERE id=?", (review, interview_id))
    con.commit()
    con.close()
