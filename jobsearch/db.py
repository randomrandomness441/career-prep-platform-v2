"""State for the job application system. Single user, SQLite, no ORM.

Same idiom as the C++ prep platform's db.py: plain stdlib sqlite3, no
framework, no ORM. Two things live here that don't in a normal CRUD app:

    point_bank   the source-of-truth career facts every generated resume
                 and cover letter pulls from -- never overwritten silently,
                 `active` flags a row as retired instead of deleting it, so
                 history and reasoning stay inspectable.
    applications the record of what was actually generated and where it
                 landed on disk, so nothing generated is ever orphaned from
                 its own folder.
"""

import difflib
import hashlib
import json
import os
import re
import sqlite3
import time
import importlib.util as _importlib_util

# Not a plain "import ats" -- when this file is loaded by server.py's own
# _load_module (importlib.util.spec_from_file_location, registered as
# "jobsearch_db", not "db"), a plain import ats can't resolve: jobsearch/
# is never added to sys.path, so Python looks for "ats" on platform/'s own
# sys.path and doesn't find it -- confirmed this breaks server.py startup
# before relying on it. Loading by this file's own directory instead works
# identically whether db.py is run directly (python3 fetch_ats.py, cwd=
# jobsearch/) or loaded by server.py's mechanism -- no sys.path dependency
# either way.
_ats_spec = _importlib_util.spec_from_file_location(
    "jobsearch_ats_for_db", os.path.join(os.path.dirname(os.path.abspath(__file__)), "ats.py"))
ats = _importlib_util.module_from_spec(_ats_spec)
_ats_spec.loader.exec_module(ats)

DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
os.makedirs(DATA_DIR, exist_ok=True)
DB_PATH = os.path.join(DATA_DIR, "jobsearch.db")

SCHEMA = """
CREATE TABLE IF NOT EXISTS point_bank (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    company     TEXT NOT NULL,
    role        TEXT NOT NULL,
    start_date  TEXT NOT NULL,
    end_date    TEXT,                  -- NULL = current role
    text        TEXT NOT NULL,         -- the accomplishment, as written
    tags        TEXT DEFAULT '[]',     -- JSON array of skill/keyword tags
    metrics     TEXT DEFAULT '[]',     -- JSON array of quantified numbers in text
    source_note TEXT DEFAULT '',       -- which resume file this came from
    active      INTEGER DEFAULT 1      -- 0 = retired/superseded, kept for history
);

CREATE TABLE IF NOT EXISTS skills (
    name     TEXT PRIMARY KEY,
    category TEXT NOT NULL,
    active   INTEGER DEFAULT 1
);

CREATE TABLE IF NOT EXISTS profile (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS jobs (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    source           TEXT NOT NULL,          -- greenhouse|lever|pinloop|manual
    company          TEXT NOT NULL,
    title            TEXT NOT NULL,
    url              TEXT DEFAULT '',
    description_raw  TEXT NOT NULL,
    fetched_ts       REAL NOT NULL,
    match_score      REAL,
    match_reason     TEXT DEFAULT '',
    status           TEXT NOT NULL DEFAULT 'new'   -- new|shortlisted|generated|applied|skipped
);

CREATE TABLE IF NOT EXISTS applications (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    job_id            INTEGER NOT NULL,
    resume_path       TEXT,
    cover_letter_path TEXT,
    ats_estimate      TEXT DEFAULT '{}',      -- JSON: {score, breakdown: [...]}
    generated_ts      REAL NOT NULL,
    folder_path       TEXT NOT NULL,
    applied_via       TEXT DEFAULT '',        -- company_site|linkedin|referral|recruiter|other
    applied_ts        REAL,                   -- NULL until actually submitted
    referral_name     TEXT DEFAULT '',
    referral_note     TEXT DEFAULT '',
    status            TEXT NOT NULL DEFAULT 'generated',
        -- generated|applied|phone_screen|interview|offer|rejected|ghosted|withdrawn
    status_history    TEXT DEFAULT '[]'       -- JSON [{status, ts, note}, ...]
);
"""


def _migrate(con):
    """Add columns to tables that existed before this field did. Idempotent:
    checks PRAGMA table_info first, never re-adds a column that's there."""
    cols = {r[1] for r in con.execute("PRAGMA table_info(jobs)").fetchall()}
    if "content_hash" not in cols:
        con.execute("ALTER TABLE jobs ADD COLUMN content_hash TEXT DEFAULT ''")
    if "llm_score" not in cols:
        # second-pass judgment from an LLM (Zai), only computed for postings
        # that already cleared the free regex filter -- NULL until that
        # runs, and stays NULL forever for postings that never qualify.
        con.execute("ALTER TABLE jobs ADD COLUMN llm_score REAL")
        con.execute("ALTER TABLE jobs ADD COLUMN llm_reason TEXT DEFAULT ''")
    if "location" not in cols:
        con.execute("ALTER TABLE jobs ADD COLUMN location TEXT DEFAULT ''")
    if "llm_judged_ts" not in cols:
        # when the LLM score was actually computed -- compared against
        # profile_version_ts() to decide if a posting needs re-judging
        # (the candidate's own data changed since it was last scored).
        con.execute("ALTER TABLE jobs ADD COLUMN llm_judged_ts REAL")
    if "posted_ts" not in cols:
        # the posting's OWN publish date, straight from the source ATS
        # (Greenhouse first_published / Lever createdAt / Ashby publishedAt)
        # -- distinct from fetched_ts (when we pulled it). Needed for the
        # fetch-time "max 1 month old" cutoff; a posting's updated_at isn't
        # safe for that since companies bump old listings without them being
        # new.
        con.execute("ALTER TABLE jobs ADD COLUMN posted_ts REAL")

    cols = {r[1] for r in con.execute("PRAGMA table_info(applications)").fetchall()}
    for name, ddl in [
        ("applied_via", "TEXT DEFAULT ''"),
        ("applied_ts", "REAL"),
        ("referral_name", "TEXT DEFAULT ''"),
        ("referral_note", "TEXT DEFAULT ''"),
        ("status", "TEXT NOT NULL DEFAULT 'generated'"),
        ("status_history", "TEXT DEFAULT '[]'"),
    ]:
        if name not in cols:
            con.execute(f"ALTER TABLE applications ADD COLUMN {name} {ddl}")
    con.commit()


def connect():
    con = sqlite3.connect(DB_PATH)
    con.row_factory = sqlite3.Row
    con.executescript(SCHEMA)
    _migrate(con)
    # Real SQL-level filtering (a WHERE clause SQLite evaluates per row while
    # scanning), not a Python loop over every row after a bare SELECT * --
    # these call the EXACT same classifier functions ats.py already has and
    # already got right (word-boundary precision for seniority/location, the
    # "senior staff before staff" ordering), so there's no second
    # hand-translated copy of that logic to drift out of sync.
    con.create_function("IS_ENGINEERING_TITLE", 1,
                         lambda title: 1 if ats.is_likely_engineering_title(title or "") else 0)
    con.create_function("SENIORITY_TIER", 1, lambda title: ats.title_seniority_tier(title or ""))
    con.create_function("IS_TARGET_LOCATION", 2,
                         lambda loc, desc: 1 if ats.is_target_location(loc, desc) else 0)
    return con


# ── profile version (for "don't re-judge unless the master doc changed") ──
_PROFILE_VERSION_KEY = "_profile_version_ts"


def _touch_profile_version():
    """Called by every point_bank/skills/profile write. A job's llm_judged_ts
    older than this means the candidate's own data changed since it was
    scored -- eligible for re-judging. Raw SQL, not profile_set(), so this
    internal bookkeeping key never shows up mixed in with user-facing
    profile fields."""
    con = connect()
    con.execute(
        "INSERT INTO profile (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
        (_PROFILE_VERSION_KEY, str(time.time())),
    )
    con.commit()
    con.close()


def profile_version_ts():
    con = connect()
    row = con.execute("SELECT value FROM profile WHERE key=?", (_PROFILE_VERSION_KEY,)).fetchone()
    con.close()
    return float(row["value"]) if row else 0.0


# ── point bank ──────────────────────────────────────────────────────────
def point_bank_add(company, role, start_date, end_date, text, tags=None,
                    metrics=None, source_note=""):
    con = connect()
    cur = con.execute(
        "INSERT INTO point_bank (company, role, start_date, end_date, text, "
        "tags, metrics, source_note) VALUES (?,?,?,?,?,?,?,?)",
        (company, role, start_date, end_date, text,
         json.dumps(tags or []), json.dumps(metrics or []), source_note),
    )
    con.commit()
    pid = cur.lastrowid
    con.close()
    _touch_profile_version()
    return pid


def point_bank_find_similar(text, threshold=0.82):
    """Best near-duplicate already in the point bank, or None. Catches the
    real case that kept happening: uploading a second resume variant
    re-extracts the same accomplishment reworded slightly, and it was
    getting added again with zero check. difflib.SequenceMatcher on
    normalized text (stdlib, no new dependency) -- not exact-match, since
    "Built X" and "Built and owned X" describing the same thing should
    still catch each other."""
    norm = re.sub(r"\s+", " ", text.strip().lower())
    best, best_ratio = None, 0.0
    for row in point_bank_all(active_only=False):
        existing_norm = re.sub(r"\s+", " ", row["text"].strip().lower())
        ratio = difflib.SequenceMatcher(None, norm, existing_norm).ratio()
        if ratio > best_ratio:
            best, best_ratio = row, ratio
    if best and best_ratio >= threshold:
        return best, best_ratio
    return None, 0.0


def point_bank_all(active_only=True):
    con = connect()
    q = "SELECT * FROM point_bank"
    if active_only:
        q += " WHERE active = 1"
    q += " ORDER BY start_date DESC, id"
    rows = [dict(r) for r in con.execute(q).fetchall()]
    con.close()
    for r in rows:
        r["tags"] = json.loads(r["tags"])
        r["metrics"] = json.loads(r["metrics"])
    return rows


def point_bank_set_active(pid, active):
    con = connect()
    con.execute("UPDATE point_bank SET active=? WHERE id=?", (1 if active else 0, pid))
    con.commit()
    con.close()
    _touch_profile_version()


def point_bank_update_text(pid, text):
    con = connect()
    con.execute("UPDATE point_bank SET text=? WHERE id=?", (text, pid))
    con.commit()
    con.close()
    _touch_profile_version()


# ── skills ──────────────────────────────────────────────────────────────
def skills_add(name, category):
    # Case-insensitive lookup -- the UNIQUE constraint on name is case-
    # sensitive, so "Embeddings" and "embeddings" used to insert as two
    # separate rows (found as real duplicate data: 6 skills double-counted
    # this way). Keeps whichever casing is already stored rather than
    # silently renaming it on a re-add with different casing.
    con = connect()
    existing = con.execute("SELECT id FROM skills WHERE LOWER(name)=LOWER(?)", (name,)).fetchone()
    if existing:
        con.execute("UPDATE skills SET category=?, active=1 WHERE id=?", (category, existing["id"]))
    else:
        con.execute("INSERT INTO skills (name, category) VALUES (?,?)", (name, category))
    con.commit()
    con.close()
    _touch_profile_version()


def skills_all(active_only=True):
    con = connect()
    q = "SELECT * FROM skills"
    if active_only:
        q += " WHERE active = 1"
    q += " ORDER BY category, name"
    rows = [dict(r) for r in con.execute(q).fetchall()]
    con.close()
    return rows


# ── profile (free-form key/value: name, email, links, summary variants) ──
def profile_set(key, value):
    con = connect()
    con.execute(
        "INSERT INTO profile (key, value) VALUES (?,?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
        (key, value),
    )
    con.commit()
    con.close()
    _touch_profile_version()


def profile_all():
    con = connect()
    rows = con.execute("SELECT key, value FROM profile").fetchall()
    con.close()
    return {r["key"]: r["value"] for r in rows if r["key"] != _PROFILE_VERSION_KEY}


# ── jobs ────────────────────────────────────────────────────────────────
def _content_hash(company, title, description_raw):
    """Same posting reposted, or the same JD pasted twice, hashes the same
    regardless of whitespace/case differences. Not sensitive to url, since
    the same job is sometimes listed at more than one url."""
    norm = re.sub(r"\s+", " ", f"{company}|{title}|{description_raw}".strip().lower())
    return hashlib.sha256(norm.encode()).hexdigest()[:16]


def jobs_find_duplicate(company, title, url, description_raw):
    """Return the existing job row if this posting is already in the table,
    else None. Two ways to match: same url (exact), or same normalized
    company+title+description (catches the same JD pasted or fetched twice,
    even from different sources)."""
    con = connect()
    row = None
    if url:
        row = con.execute("SELECT * FROM jobs WHERE url=? AND url != ''", (url,)).fetchone()
    if row is None:
        h = _content_hash(company, title, description_raw)
        row = con.execute("SELECT * FROM jobs WHERE content_hash=?", (h,)).fetchone()
    con.close()
    return dict(row) if row else None


def jobs_add(source, company, title, url, description_raw, location="", posted_ts=None):
    """Returns (job_id, is_new). is_new=False means this exact posting was
    already in the table -- the existing row's id comes back, nothing new
    is inserted, so callers can't silently double-track the same job.

    If the existing row has no location on file yet and this call has one,
    backfills it in place -- lets a plain re-run of the fetch fill in
    location for postings that were tracked before location was captured
    at all, with no separate migration script needed."""
    dup = jobs_find_duplicate(company, title, url, description_raw)
    if dup:
        if location and not dup.get("location"):
            con = connect()
            con.execute("UPDATE jobs SET location=? WHERE id=?", (location, dup["id"]))
            con.commit()
            con.close()
        return dup["id"], False

    con = connect()
    h = _content_hash(company, title, description_raw)
    cur = con.execute(
        "INSERT INTO jobs (source, company, title, url, description_raw, fetched_ts, content_hash, location, posted_ts) "
        "VALUES (?,?,?,?,?,?,?,?,?)",
        (source, company, title, url, description_raw, time.time(), h, location, posted_ts),
    )
    con.commit()
    jid = cur.lastrowid
    con.close()
    return jid, True


def jobs_list(status=None):
    con = connect()
    if status:
        rows = con.execute(
            "SELECT * FROM jobs WHERE status=? ORDER BY llm_score DESC NULLS LAST, fetched_ts DESC",
            (status,),
        ).fetchall()
    else:
        rows = con.execute(
            "SELECT * FROM jobs ORDER BY llm_score DESC NULLS LAST, fetched_ts DESC"
        ).fetchall()
    con.close()
    return [dict(r) for r in rows]


def jobs_query(status=None, eng_only=True, level_tiers=None, min_score=None, q_text="",
               company_filter="", source_filter="", date_cutoff=None, exclude_applied=True):
    """The real filter for the Postings list -- one SQL query with a WHERE
    clause SQLite evaluates per row while scanning, not a SELECT * pulled
    into Python followed by a manual loop over every row (what this
    replaced). IS_ENGINEERING_TITLE/SENIORITY_TIER/IS_TARGET_LOCATION are
    registered on the connection in connect() -- same classifier functions
    ats.py already has, called per-row by SQLite itself, so there's no
    second hand-translated copy of that logic (word-boundary precision,
    "senior staff" before "staff" ordering) to drift out of sync.

    India/Bangalore/Bengaluru location is unconditional, not a parameter --
    this whole app only ever targets that location now.

    Returns (total_unfiltered, matched_jobs) -- matched_jobs already sorted
    by LLM score descending, NULLS LAST (unjudged postings sort after judged
    ones, by fetch date)."""
    con = connect()

    unfiltered_q, unfiltered_params = "SELECT COUNT(*) FROM jobs", []
    if status:
        unfiltered_q += " WHERE status=?"
        unfiltered_params.append(status)
    total_unfiltered = con.execute(unfiltered_q, unfiltered_params).fetchone()[0]

    where, params = ["IS_TARGET_LOCATION(location, description_raw) = 1"], []
    if status:
        where.append("status=?")
        params.append(status)
    if eng_only:
        where.append("IS_ENGINEERING_TITLE(title) = 1")
    if level_tiers:
        where.append(f"SENIORITY_TIER(title) IN ({','.join('?' * len(level_tiers))})")
        params.extend(level_tiers)
    if min_score is not None:
        # NULL llm_score (not yet AI-judged) always passes -- "llm_score >= ?"
        # alone would silently drop every unjudged posting, since SQL compares
        # against NULL as false, which is the opposite of what a min-score
        # filter should do before judging has even run.
        where.append("(llm_score IS NULL OR llm_score >= ?)")
        params.append(float(min_score))
    if q_text:
        where.append("(LOWER(company) LIKE ? OR LOWER(title) LIKE ?)")
        like = f"%{q_text}%"
        params.extend([like, like])
    if company_filter:
        where.append("LOWER(company) = ?")
        params.append(company_filter)
    if source_filter == "manual":
        where.append("source = 'manual'")
    elif source_filter == "auto":
        where.append("source != 'manual'")
    if date_cutoff is not None:
        where.append("fetched_ts >= ?")
        params.append(date_cutoff)
    if exclude_applied:
        where.append("id NOT IN (SELECT DISTINCT job_id FROM applications WHERE status != 'generated')")

    query = ("SELECT * FROM jobs WHERE " + " AND ".join(where) +
              " ORDER BY llm_score DESC NULLS LAST, fetched_ts DESC")
    rows = con.execute(query, params).fetchall()
    con.close()
    return total_unfiltered, [dict(r) for r in rows]


def jobs_get(jid):
    con = connect()
    row = con.execute("SELECT * FROM jobs WHERE id=?", (jid,)).fetchone()
    con.close()
    return dict(row) if row else None




def jobs_set_llm_score(jid, score, reason):
    con = connect()
    con.execute("UPDATE jobs SET llm_score=?, llm_reason=?, llm_judged_ts=? WHERE id=?",
                (score, reason, time.time(), jid))
    con.commit()
    con.close()


def jobs_llm_score_stale(job):
    """True if this posting has never been judged, or was judged before the
    candidate's own point bank/skills/profile last changed -- the "don't
    re-judge unless the master doc changed" rule. A judged posting whose
    score predates the latest profile edit is treated the same as an
    unjudged one; everything else is left alone."""
    if job["llm_score"] is None:
        return True
    judged_ts = job["llm_judged_ts"] or 0
    return judged_ts < profile_version_ts()


def jobs_clear_llm_scores(date_cutoff=None):
    """Wipe llm_score/llm_reason/llm_judged_ts -- the explicit "force
    reload" the UI offers, for when the profile changed enough that you
    want a clean re-judge pass rather than relying on the staleness check
    alone. date_cutoff (optional, a fetched_ts): only clears postings
    fetched at or after it, so clearing doesn't also make a posting from
    months back newly eligible for a real LLM spend on the next judge pass.
    Returns how many rows were actually cleared."""
    con = connect()
    if date_cutoff is not None:
        cur = con.execute(
            "UPDATE jobs SET llm_score=NULL, llm_reason='', llm_judged_ts=NULL "
            "WHERE llm_score IS NOT NULL AND fetched_ts >= ?",
            (date_cutoff,),
        )
    else:
        cur = con.execute(
            "UPDATE jobs SET llm_score=NULL, llm_reason='', llm_judged_ts=NULL WHERE llm_score IS NOT NULL"
        )
    con.commit()
    n = cur.rowcount
    con.close()
    return n


def jobs_set_status(jid, status):
    con = connect()
    con.execute("UPDATE jobs SET status=? WHERE id=?", (status, jid))
    con.commit()
    con.close()


# ── applications ────────────────────────────────────────────────────────
def applications_add(job_id, resume_path, cover_letter_path, ats_estimate, folder_path):
    con = connect()
    cur = con.execute(
        "INSERT INTO applications (job_id, resume_path, cover_letter_path, "
        "ats_estimate, generated_ts, folder_path) VALUES (?,?,?,?,?,?)",
        (job_id, resume_path, cover_letter_path, json.dumps(ats_estimate),
         time.time(), folder_path),
    )
    con.commit()
    aid = cur.lastrowid
    con.close()
    return aid


def applications_find_draft(job_id):
    """The most recent application for this job still at status='generated'
    -- materials exist but nothing was actually submitted yet. Used before
    a regenerate so a second draft replaces the first instead of leaving
    two rows for the same job (status != 'generated' -- a real submitted
    application -- is never touched this way; that's a real record, not a
    draft to overwrite)."""
    con = connect()
    row = con.execute(
        "SELECT * FROM applications WHERE job_id=? AND status='generated' "
        "ORDER BY generated_ts DESC LIMIT 1", (job_id,)
    ).fetchone()
    con.close()
    return dict(row) if row else None


def applications_delete(app_id):
    con = connect()
    con.execute("DELETE FROM applications WHERE id=?", (app_id,))
    con.commit()
    con.close()


def applications_for_job(job_id):
    con = connect()
    rows = con.execute(
        "SELECT * FROM applications WHERE job_id=? ORDER BY generated_ts DESC", (job_id,)
    ).fetchall()
    con.close()
    out = []
    for r in rows:
        d = dict(r)
        d["ats_estimate"] = json.loads(d["ats_estimate"])
        d["status_history"] = json.loads(d["status_history"])
        out.append(d)
    return out


def _application_row(con, app_id):
    row = con.execute("SELECT * FROM applications WHERE id=?", (app_id,)).fetchone()
    if row is None:
        raise ValueError(f"no application with id={app_id}")
    return row


def applications_already_applied(job_id):
    """True if any application for this job has moved past 'generated' --
    i.e. it was actually submitted at some point. The dedup check before
    marking applied again, or before regenerating without noticing."""
    con = connect()
    row = con.execute(
        "SELECT 1 FROM applications WHERE job_id=? AND status != 'generated' LIMIT 1",
        (job_id,),
    ).fetchone()
    con.close()
    return row is not None


def applications_all_applied_job_ids():
    """Every job_id with at least one application past 'generated' -- one
    query for the whole set, so job-list filtering doesn't run
    applications_already_applied() once per row."""
    con = connect()
    rows = con.execute("SELECT DISTINCT job_id FROM applications WHERE status != 'generated'").fetchall()
    con.close()
    return {r["job_id"] for r in rows}


def applications_mark_applied(app_id, applied_via, referral_name="", referral_note=""):
    """Record the real-world moment of submitting. applied_via: company_site|
    linkedin|referral|recruiter|other."""
    con = connect()
    row = _application_row(con, app_id)
    hist = json.loads(row["status_history"])
    hist.append({"status": "applied", "ts": time.time(), "note": f"via {applied_via}"})
    con.execute(
        "UPDATE applications SET applied_via=?, applied_ts=?, referral_name=?, "
        "referral_note=?, status='applied', status_history=? WHERE id=?",
        (applied_via, time.time(), referral_name, referral_note, json.dumps(hist), app_id),
    )
    con.commit()
    con.close()


def applications_quick_mark_applied(job_id, applied_via, referral_name="", referral_note=""):
    """Mark a job applied WITHOUT going through the generate pipeline --
    for the real, common case of applying directly on the company's site or
    LinkedIn, with your own resume, never touching this tool's drafting
    flow at all. Creates a real applications row (empty resume/cover-letter/
    folder_path -- there are no generated materials for this one) with
    status='applied' from the start, so it's picked up by the exact same
    exclude-from-postings and Applications-tab logic a generated-then-marked
    application already uses -- no second, parallel "applied" concept to
    keep in sync with this one."""
    con = connect()
    now = time.time()
    hist = [{"status": "applied", "ts": now, "note": f"via {applied_via}"}]
    cur = con.execute(
        "INSERT INTO applications (job_id, resume_path, cover_letter_path, ats_estimate, "
        "generated_ts, folder_path, applied_via, applied_ts, referral_name, referral_note, "
        "status, status_history) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)",
        (job_id, "", "", "{}", now, "", applied_via, now, referral_name, referral_note,
         "applied", json.dumps(hist)),
    )
    con.commit()
    aid = cur.lastrowid
    con.close()
    return aid


def applications_add_status_event(app_id, status, note=""):
    """Append a stage to the funnel: phone_screen, interview, offer,
    rejected, ghosted, withdrawn. Keeps the full history, not just the
    latest stage, so 'what actually happened' is never lost."""
    con = connect()
    row = _application_row(con, app_id)
    hist = json.loads(row["status_history"])
    hist.append({"status": status, "ts": time.time(), "note": note})
    con.execute(
        "UPDATE applications SET status=?, status_history=? WHERE id=?",
        (status, json.dumps(hist), app_id),
    )
    con.commit()
    con.close()


def applications_all():
    con = connect()
    rows = con.execute(
        "SELECT a.*, j.company, j.title FROM applications a "
        "JOIN jobs j ON j.id = a.job_id ORDER BY a.generated_ts DESC"
    ).fetchall()
    con.close()
    out = []
    for r in rows:
        d = dict(r)
        d["ats_estimate"] = json.loads(d["ats_estimate"])
        d["status_history"] = json.loads(d["status_history"])
        out.append(d)
    return out
