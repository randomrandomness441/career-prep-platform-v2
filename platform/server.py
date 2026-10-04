#!/usr/bin/env python3
"""Local prep platform. Single user, no dependencies outside the stdlib.

    ./platform/server.py        then open http://localhost:8777

HTTP + SSE for the app, and a hand-rolled WebSocket carrying a real pty so the
terminal in the browser is an actual shell, not a fake one.
"""

import base64
import hashlib
import importlib.util
import json
import os
import pty
import re
import select
import signal
import struct
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import db
import runner
import java_runner

ROOT = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(ROOT)
STATIC = os.path.join(ROOT, "static")
QUESTIONS = os.path.join(ROOT, "questions")
PORT = int(os.environ.get("PORT", "8777"))

# ── jobsearch/ integration ────────────────────────────────────────────────
# jobsearch/ has its own db.py -- loaded under a distinct sys.modules name
# ("jobsearch_db") via importlib rather than a plain `import db`, since that
# name is already bound above to *this* app's own db.py; a second plain
# import would silently reuse the cached module instead of loading the
# right file. jobsearch's db.py and ats.py are pure stdlib (no python-docx),
# so both are safe to import straight into this process. Generation (which
# needs python-docx, installed only in jobsearch/.venv) is NOT imported
# here -- it runs as a subprocess via JOBSEARCH_VENV_PY, same reasoning.
# Two real layouts this has to resolve correctly: on this machine, jobsearch/
# sits next to cpp/ (REPO's own parent), a holdover from before the two
# projects were merged into one served app. A fresh clone of the public
# snapshot repo is flat instead -- platform/ and jobsearch/ as direct
# siblings (REPO itself). Try the flat layout first since it's the one a new
# user actually has; it only resolves to something real there, so this
# changes nothing about where the live app on THIS machine looks.
_flat_jobsearch = os.path.join(REPO, "jobsearch")
JOBSEARCH_DIR = _flat_jobsearch if os.path.isdir(_flat_jobsearch) else os.path.join(os.path.dirname(REPO), "jobsearch")
JOBSEARCH_STATIC = os.path.join(JOBSEARCH_DIR, "static")
JOBSEARCH_APPLICATIONS = os.path.join(JOBSEARCH_DIR, "applications")
JOBSEARCH_VENV_PY = os.path.join(JOBSEARCH_DIR, ".venv", "bin", "python3")


def _load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


jobsdb = _load_module("jobsearch_db", os.path.join(JOBSEARCH_DIR, "db.py"))
jobs_ats = _load_module("jobsearch_ats", os.path.join(JOBSEARCH_DIR, "ats.py"))
# Pure stdlib (re only) -- loads fine under this process's plain python3,
# no need to shell into the jobsearch venv just to run the style check.
jobs_humanizer = _load_module("jobsearch_humanizer", os.path.join(JOBSEARCH_DIR, "humanizer.py"))


def _jobs_resume_body():
    """(profile, skill_names, bullets_text, skills_line_text, candidate_years)
    -- the untailored candidate baseline, used to build the LLM-judge
    prompt (the only place a real fit judgment happens -- regex only ever
    does title/seniority/location/date classification, never scoring).
    bullets_text and skills_line_text are kept separate so the judge prompt
    can tell a skill demonstrated in real point-bank prose apart from one
    only present in the bare Skills line."""
    profile = jobsdb.profile_all()
    skill_names = [s["name"] for s in jobsdb.skills_all()]
    points = [p for p in jobsdb.point_bank_all() if not p["text"].lstrip().startswith("[")]
    bullets_text = "\n".join(p["text"] for p in points)
    skills_line_text = ", ".join(skill_names)
    candidate_years = float(profile.get("years_experience", 0) or 0)
    return profile, skill_names, bullets_text, skills_line_text, candidate_years


def _job_public(j, applied_ids):
    # "stale" (judged, but your master doc changed since) is deliberately
    # distinct from "never judged" -- jobs_llm_score_stale() returns True
    # for both, but the UI already has its own "not judged" treatment for
    # the never-judged case, so only surface the badge for the case that's
    # actually new information: a score you're looking at right now that
    # no longer reflects your current profile. There's no separate stored
    # flag to go out of sync -- this is computed fresh from llm_judged_ts
    # vs profile_version_ts() on every read, and llm_judged_ts only moves
    # when THAT posting is individually re-judged, so it can't clear any
    # other way.
    stale = j["llm_score"] is not None and jobsdb.jobs_llm_score_stale(j)
    return {
        "id": j["id"], "company": j["company"], "title": j["title"], "url": j["url"],
        "source": j["source"], "status": j["status"],
        "fetched_ts": j["fetched_ts"],
        "is_engineering": jobs_ats.is_likely_engineering_title(j["title"]),
        "seniority_tier": jobs_ats.title_seniority_tier(j["title"]),
        "llm_score": j["llm_score"], "llm_reason": j["llm_reason"], "stale": stale,
        "location": j["location"],
        "applied": j["id"] in applied_ids,
    }


def _day_start_ts(days_ago):
    """Midnight, local time, N days ago -- for the "fetched today / this
    week" filter, so a posting that's been open for weeks doesn't keep
    resurfacing as if it just showed up."""
    t = time.localtime()
    midnight = time.mktime((t.tm_year, t.tm_mon, t.tm_mday, 0, 0, 0, 0, 0, -1))
    return midnight - days_ago * 86400


def _filter_jobs(qs):
    """Shared by /api/jobs/list and /api/jobs/companies so the two always
    agree on what "matching" means -- a company's card count/best-score and
    the list you get after clicking into it must be the same filter,
    computed once, not two versions that can drift apart.

    This is a thin param-parsing layer over jobsdb.jobs_query -- the actual
    filtering is a SQL WHERE clause now, not a Python loop over every row
    (what this used to be). Returns (total_unfiltered, matched_jobs),
    matched_jobs already sorted by effective score descending."""
    min_score = qs.get("min_score", [None])[0]
    eng_only = qs.get("eng_only", ["1"])[0] == "1"
    q_text = (qs.get("q", [""])[0] or "").lower()
    # level: "core" (mid+senior -- the realistic target band) is the
    # default; "stretch" also allows staff/senior_staff; "all" adds
    # manager. Not a score penalty like years-of-experience -- this is a
    # different tier of role entirely, not a worse fit for the same tier,
    # so it's a filter, not a point deduction.
    level = qs.get("level", ["core"])[0]
    level_tiers = {
        "core": {"mid", "senior"},
        "stretch": {"mid", "senior", "staff", "senior_staff"},
        "all": {"mid", "senior", "staff", "senior_staff", "manager"},
    }.get(level, {"mid", "senior"})
    company_filter = (qs.get("company", [""])[0] or "").lower()
    source_filter = qs.get("source", [""])[0]   # "" (any) | "manual" | "auto"
    date_filter = qs.get("date", [""])[0]   # "" (any) | "today" | "week" | "2weeks"
    date_cutoff = {"today": _day_start_ts(0), "week": _day_start_ts(7),
                    "2weeks": _day_start_ts(14)}.get(date_filter)

    return jobsdb.jobs_query(
        status=qs.get("status", [None])[0], eng_only=eng_only, level_tiers=level_tiers,
        min_score=min_score, q_text=q_text, company_filter=company_filter,
        source_filter=source_filter, date_cutoff=date_cutoff, exclude_applied=True)

# Mock interview, live: the interviewer ("Voss") is backed by whichever LLM
# provider LLM_PROVIDER names ("anthropic" or "zai") -- flip that one env
# var to switch, nothing else changes. Set the matching *_API_KEY in your own
# shell before running this server (never paste it into the app or into
# chat). Without a key for the active provider, /api/interview/* still works
# -- it just posts nothing automatically, same as before either existed.
LLM_PROVIDER = os.environ.get("LLM_PROVIDER", "anthropic")   # "anthropic" | "zai"
# Both providers' default temperature (1.0) is tuned for open-ended
# generation, not a consistent, rigorous interviewer -- dialed down
# deliberately for either.
VOSS_TEMPERATURE = float(os.environ.get("VOSS_TEMPERATURE", "0.5"))

ANTHROPIC_API_KEY = os.environ.get("ANTHROPIC_API_KEY", "")
ANTHROPIC_MODEL = os.environ.get("ANTHROPIC_MODEL", "claude-sonnet-5")   # Voss persona only -- unchanged
# Fallback JSON-task tier (judge/draft/extract via _llm_json_call), both the
# real-API and CLI branches -- Haiku 4.5, per Anthropic's own documented
# guidance ("Choose Haiku for simple tasks, Sonnet for most production
# workloads" -- platform.claude.com/docs/en/about-claude/pricing). Half the
# price of Sonnet on both input and output tokens. Mirrors ZAI_MODEL/
# ZAI_MODEL_FULL below for the same reason: these are high-volume, single-
# shot JSON judgments, not an open-ended conversation like Voss.
ANTHROPIC_MODEL_FALLBACK = os.environ.get("ANTHROPIC_MODEL_FALLBACK", "claude-haiku-4-5-20251001")
# Reserved for the one call site writing into the master document itself
# (_zai_rewrite_point) -- same reasoning as ZAI_MODEL_FULL.
ANTHROPIC_MODEL_FULL = os.environ.get("ANTHROPIC_MODEL_FULL", "claude-sonnet-5")

ZAI_API_KEY = os.environ.get("ZAI_API_KEY", "")
ZAI_MODEL = os.environ.get("ZAI_MODEL", "glm-5.3-flash")
# Full (non-Flash) GLM-5.3, reserved for the one call site where a bad
# answer costs the most: rewriting what actually goes on the master résumé
# document (_zai_rewrite_point). Every other call site -- job judging,
# drafting, résumé extraction, the Voss interview persona -- runs on Flash
# above: ~13-20x cheaper per Z.ai's own pricing (docs.z.ai, checked live),
# and the volume that matters here (judging hundreds of postings in a
# batch) is exactly where that difference adds up.
ZAI_MODEL_FULL = os.environ.get("ZAI_MODEL_FULL", "glm-5.3")
# /coding/paas/v4/ specifically -- that's the endpoint the GLM Coding Lite
# subscription actually covers. The general /api/paas/v4/ (no "coding")
# bills from a separate pay-as-you-go wallet and 429s "Insufficient
# balance" the moment that's empty, even with an active coding-plan key --
# same models, different billing rail. Confirmed by the exact error hit.
ZAI_URL = "https://api.z.ai/api/coding/paas/v4/chat/completions"
# This comment used to claim thinking can't be disabled for glm-5.3 --
# tested that directly against the live API (thinking:"disabled", a plain
# prompt) and got back completion_tokens_details.reasoning_tokens == 0, so
# it's actually honored for this model too. Leaving the note here since the
# claim was wrong once and is worth re-checking if Z.ai's behavior changes.


# ── question loading ─────────────────────────────────────────────────────
def _read(path, default=""):
    try:
        with open(path) as f:
            return f.read()
    except OSError:
        return default


def load_questions():
    out = []
    if not os.path.isdir(QUESTIONS):
        return out
    for topic in sorted(os.listdir(QUESTIONS)):
        tdir = os.path.join(QUESTIONS, topic)
        if not os.path.isdir(tdir):
            continue
        for slug in sorted(os.listdir(tdir)):
            qdir = os.path.join(tdir, slug)
            meta_path = os.path.join(qdir, "meta.json")
            if not os.path.isfile(meta_path):
                continue
            meta = json.loads(_read(meta_path, "{}"))
            parts_dir = os.path.join(qdir, "parts")
            n = len([d for d in os.listdir(parts_dir)
                     if os.path.isdir(os.path.join(parts_dir, d))]) if os.path.isdir(parts_dir) else 0
            meta.update({"id": slug, "topic": topic, "dir": qdir, "parts": max(n, 1)})
            out.append(meta)
    return out


def part_payload(q, part):
    pdir = os.path.join(q["dir"], "parts", str(part))
    saved = db.load_code(q["id"], part)
    kind = q.get("kind", "compiled")
    lang = q.get("lang", "cpp")
    boiler_name = "boilerplate.java" if lang == "java" else "boilerplate.cpp"
    boiler = _read(os.path.join(pdir, boiler_name))
    hints = [h.strip() for h in _read(os.path.join(pdir, "hints.md")).split("\n---\n") if h.strip()]
    st = db.part_status(q["id"], part)
    # A judged question has no solution file -- rubric.md is its model-answer equivalent.
    solution_name = "rubric.md" if kind == "judged" else ("solution.java" if lang == "java" else "solution.cpp")
    has_solution = os.path.isfile(os.path.join(pdir, solution_name))
    stars = db.stars_all()
    return {
        "meta": {k: q[k] for k in q if k != "dir"},
        "part": part,
        "statement": _read(os.path.join(pdir, "statement.md")),
        # Optional: a minimal illustrative main()-shaped snippet showing how
        # the interface gets called (which threads call which methods, in
        # what shape) -- NOT the real test harness. tests.cpp itself is
        # never sent to the client at all, so the actual assertions stay a
        # surprise; this file exists purely so the calling convention isn't
        # something you have to guess.
        "usage": _read(os.path.join(pdir, "usage.md")),
        "reading": _read(os.path.join(q["dir"], "reading.md")),
        "part_reading": _read(os.path.join(pdir, "reading.md")),
        "code": saved if saved is not None else boiler,
        "boilerplate": boiler,
        "kind": kind,
        "lang": lang,
        "hint_count": len(hints),
        "hints": hints,           # client only renders revealed ones
        "has_solution": has_solution,
        # Once legitimately revealed, send the text back on every load --
        # not just the one-time /api/reveal response -- so a page reload
        # or revisit shows it again instead of a dead "click reveal again"
        # placeholder with no button left to click.
        "solution": (_read(os.path.join(pdir, solution_name))
                     if has_solution and st["saw_solution"] else None),
        "starred": q["id"] in stars,
        "star_note": stars.get(q["id"], ""),
        "status": st,
        "label": db.label(st),
    }


def find_question(qid):
    for q in load_questions():
        if q["id"] == qid:
            return q
    return None


def load_pack(topic):
    """Per-topic-folder persona/branding: questions/<topic>/pack.json. Missing file or
    missing fields fall back to defaults that reproduce this platform's original
    hardcoded persona text, so content that predates pack.json needs no changes."""
    defaults = {
        "id": topic, "name": topic.replace("-", " ").title() if topic else "This platform",
        "topic_phrase": "this material", "teacher_framing": "this course",
        "interviewer_name": "Voss", "interviewer_role": "senior software engineer",
    }
    try:
        data = json.loads(_read(os.path.join(QUESTIONS, topic, "pack.json"), "{}"))
    except json.JSONDecodeError:
        data = {}
    defaults.update(data)
    return defaults


# ── Voss: the live-in-app interviewer ─────────────────────────────────────
# Backed by whichever provider LLM_PROVIDER names (stdlib urllib either way,
# no new dependency). Every reply is generated in a background thread so
# /api/interview/* keeps returning immediately -- the browser's existing
# poll loop just starts picking up real replies instead of needing a human
# to post them by hand.

# Shared by every persona below -- what actually makes text read as AI-generated
# (em dashes, "not X but Y", hedging) rather than a person talking.
HUMAN_STYLE_RULES = """
How you talk (write like a person speaking, not like an AI assistant):
- One idea per sentence. If you catch yourself adding a parenthetical or a clause after
  a comma to sneak in a second idea, stop and make it its own sentence instead.
- Plain connecting words only: and, but, so, because. Never "furthermore", "moreover",
  "additionally".
- Never use an em dash. Use a period or a comma.
- Never write "it's not just X, it's Y".
- Never use "delve", "leverage", "underscore", "boast", "robust", "seamless".
- Say things directly. Don't hedge with "can be complex" or "it's worth noting that".
- Don't close with "let me know if you have any questions" or similar.
"""

def voss_persona(pack):
    return f"""You are {pack['interviewer_name']}, a {pack['interviewer_role']} running a live \
technical interview on {pack['topic_phrase']}. You are rigorous, terse, and warm only when \
it's earned -- never a cheerleader, never sarcastic.

How you run the interview, in order:
1. Clarify constraints -- make sure the candidate has actually restated the problem
   correctly before they write anything.
2. Once they have an approach, make them trace a concrete example by hand rather than
   letting them jump straight to code.
3. Probe a real edge case specific to this problem.
4. Challenge their complexity or correctness claim directly ("is that actually true?")
   -- don't just accept an assertion.
5. Once the core solution is sound, explore a trade-off or alternative design.

Rules:
- Guide with questions, don't hand them the fix. If they say something vague like "it
  just handles it" or "somehow it works", press on exactly that.
- One question at a time -- never stack several in one message.
- React to their actual code (given to you below, refreshed every turn) as well as what
  they say, not just their words.
- Keep replies short: two to five sentences, like real interview dialogue, not an essay.
- If they ask a real clarifying question, just answer it plainly and move on.
- You will be told explicitly when it's time to close the interview and write the
  review -- follow that instruction's format exactly when it comes, and only then.
""" + HUMAN_STYLE_RULES


def teacher_persona(pack):
    return f"""You are the teacher for {pack['teacher_framing']}. A student is
reading about a question and something in it isn't clicking yet.

How you teach:
- Explain like you're talking to a smart person who has never seen this idea before.
  Short sentences. Everyday words.
- Use one plain, everyday analogy and stay inside it. Don't switch analogies halfway
  through.
- Go concrete before abstract. Show what actually happens, then name the concept.
- Default to a hint, not the answer. Ask the question that makes them see it themselves,
  the way {pack['interviewer_name']} would, even though you're not running an interview here.
- Only give the direct answer once they've clearly asked a second time in the same
  thread, past your hint. At that point just tell them plainly. Don't make them ask
  three times or guess a magic phrase.
- Never quote the reading material at them word for word. They can already see that
  text. Say it again in your own words, differently, or you haven't taught them
  anything.
- If their question reveals a specific wrong idea, name the wrong idea and fix it.
  Don't just restate the right answer next to it and hope they notice the difference.
- Keep answers short, a few sentences to a short paragraph, unless they clearly asked
  for a longer walkthrough.
""" + HUMAN_STYLE_RULES


def judge_persona(pack):
    return f"""You are grading one finished written answer for {pack['teacher_framing']}, \
against a rubric. You are not chatting with the student -- this is a one-shot judgement, not
a conversation.

Rules:
- Be strict but fair. A vague answer that gestures at the right idea without committing to
  specifics is NEEDS_WORK, not PASS. A wrong claim stated confidently is NEEDS_WORK even if
  the general direction is right.
- Point at the specific part of the rubric they did or didn't address. Don't just say
  "needs more detail" with nothing to attach it to.
- Reply in exactly this shape: the first line is exactly PASS or NEEDS_WORK and nothing
  else, then a short written verdict naming what's solid and what's missing or wrong.
""" + HUMAN_STYLE_RULES


def _question_context(qid, part):
    q = find_question(qid)
    if not q:
        return "(question not found)"
    pdir = os.path.join(q["dir"], "parts", str(part))
    statement = _read(os.path.join(pdir, "statement.md"))
    reading = _read(os.path.join(pdir, "reading.md")) or _read(os.path.join(q["dir"], "reading.md"))
    solution = _read(os.path.join(pdir, "solution.cpp"))
    return (
        f"## The problem you're interviewing the candidate on\n\n{statement}\n\n"
        f"## Background on this problem, for your own grounding -- never quote or hint "
        f"at this directly\n\n{reading}\n\n"
        f"## Reference solution, for your own grounding -- never reveal or describe its "
        f"exact shape\n\n```cpp\n{solution or '(no reference solution on file)'}\n```"
    )


def _system_prompt(qid, part, code):
    q = find_question(qid)
    pack = load_pack(q["topic"] if q else "")
    return (voss_persona(pack) + "\n\n" + _question_context(qid, part) +
            f"\n\n## The candidate's code right now\n\n```cpp\n{code or '(nothing written yet)'}\n```")


def _transcript_messages(iid, extra=None):
    rows = db.interview_messages_since(iid, 0)
    out = [{"role": "user" if r["role"] == "candidate" else "assistant", "content": r["text"]} for r in rows]
    if extra:
        out.append(extra)
    return out


def _anthropic_call(system_prompt, messages, max_tokens=1200, temperature=None, model=None):
    if not ANTHROPIC_API_KEY:
        print("[voss] ANTHROPIC_API_KEY not set -- skipping automatic reply", file=sys.stderr)
        return None
    body = json.dumps({
        "model": model or ANTHROPIC_MODEL,
        "max_tokens": max_tokens,
        "temperature": VOSS_TEMPERATURE if temperature is None else temperature,
        # cache_control: the persona+question context repeats verbatim on every
        # call within an interview, so it's the part worth caching.
        "system": [{"type": "text", "text": system_prompt, "cache_control": {"type": "ephemeral"}}],
        "messages": messages,
    }).encode()
    req = urllib.request.Request(
        "https://api.anthropic.com/v1/messages", data=body, method="POST",
        headers={"x-api-key": ANTHROPIC_API_KEY, "anthropic-version": "2023-06-01",
                 "content-type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            data = json.loads(resp.read())
        return "".join(b.get("text", "") for b in data.get("content", []) if b.get("type") == "text").strip()
    except urllib.error.HTTPError as e:
        print(f"[voss] Anthropic API error {e.code}: {e.read().decode(errors='replace')}", file=sys.stderr)
    except Exception as e:
        print(f"[voss] Anthropic API call failed: {e}", file=sys.stderr)
    return None


# Last-resort Claude access with no billed API key: shells out to the
# `claude` CLI itself, authenticated with a long-lived OAuth token from
# `claude setup-token` (CLAUDE_CODE_OAUTH_TOKEN) instead of ANTHROPIC_API_KEY
# -- draws on the Claude subscription's own usage window, not a separate
# dollar-metered account. This is the same officially-documented mechanism
# Anthropic's own claude-code-action GitHub Action uses for CI (code.claude.
# com/docs/en/authentication), not a scraping/reverse-engineering workaround.
#
# Real, measured cost of this path: even with every optional feature
# disabled below, a trivial call still carries ~$0.02-0.04 of *notional*
# list-price-equivalent overhead per call (Claude Code's own harness -- its
# built-in tool schemas and agent system prompt -- gets reloaded every
# invocation; --bare would strip that but explicitly refuses OAuth, requiring
# a real API key instead, which defeats the point). On a subscription that
# isn't a real dollar cost, but it IS real usage-window quota -- the same
# pool an actual interactive coding session draws from -- plus ~1.5s of
# process-spawn latency per call. That's why this is the LAST tier tried,
# never primary, and why ZAI_MODEL defaults to Flash (see above): the
# cheap/fast tier should absorb as much real volume as possible so this
# one only ever has to cover the rare multi-hour outage.
CLAUDE_CLI_OAUTH_TOKEN = os.environ.get("CLAUDE_CODE_OAUTH_TOKEN", "")
CLAUDE_CLI_TIMEOUT = 90
# Every tool a coding session would need is irrelevant to (and only adds
# risk/latency to) a plain "answer this JSON question" call -- denying them
# explicitly is what earned the ~57% token-overhead cut measured live versus
# the harness defaults (21.9K -> 9.4K cache-creation tokens on an identical
# trivial prompt).
_CLAUDE_CLI_DISALLOWED_TOOLS = "Bash,Read,Write,Edit,Glob,Grep,WebSearch,WebFetch,NotebookEdit,Task"


def _flatten_messages(messages):
    """The CLI fallback is one stateless `claude -p <prompt>` call, no
    --continue -- so a multi-turn messages list (the humanizer-retry loop
    in _zai_draft_application appends assistant/user turns) has to become
    one plain-text block instead of a real conversation. Fine for prompts
    that are already self-contained exchanges, which every caller here is."""
    if len(messages) == 1 and messages[0]["role"] == "user":
        return messages[0]["content"]
    return "\n\n".join(f"{'Assistant' if m['role'] == 'assistant' else 'User'}: {m['content']}"
                        for m in messages)


def _claude_cli_call(system_prompt, messages, max_tokens=1200, temperature=None, model=None):
    if not CLAUDE_CLI_OAUTH_TOKEN:
        return None
    cmd = ["claude", "-p", _flatten_messages(messages), "--output-format", "json",
           "--system-prompt", system_prompt, "--model", model or ANTHROPIC_MODEL_FALLBACK,
           "--disable-slash-commands", "--strict-mcp-config",
           "--disallowedTools", _CLAUDE_CLI_DISALLOWED_TOOLS]
    env = dict(os.environ, CLAUDE_CODE_OAUTH_TOKEN=CLAUDE_CLI_OAUTH_TOKEN)
    try:
        out = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=CLAUDE_CLI_TIMEOUT)
    except Exception as e:
        print(f"[llm] Claude CLI fallback failed to run: {e}", file=sys.stderr)
        return None
    if out.returncode != 0:
        print(f"[llm] Claude CLI fallback exited {out.returncode}: {out.stderr[:300]}", file=sys.stderr)
        return None
    try:
        # --output-format json prints exactly one JSON object -- but take
        # the last non-blank line defensively in case anything else (a
        # warning banner, like the OAuth one seen in real testing) lands
        # on stdout ahead of it.
        lines = [l for l in out.stdout.splitlines() if l.strip()]
        data = json.loads(lines[-1])
        if data.get("is_error"):
            print(f"[llm] Claude CLI fallback returned an error result: {str(data)[:300]}", file=sys.stderr)
            return None
        return data.get("result")
    except Exception as e:
        print(f"[llm] Claude CLI fallback reply wasn't parseable JSON: {e} -- raw: {out.stdout[:300]}",
              file=sys.stderr)
        return None


def _zai_call(system_prompt, messages, max_tokens=1200, temperature=None, thinking="enabled",
              response_format=None, model=None):
    if not ZAI_API_KEY:
        print("[voss] ZAI_API_KEY not set -- skipping automatic reply", file=sys.stderr)
        return None
    body_obj = {
        "model": model or ZAI_MODEL,
        # reasoning tokens draw from this SAME budget when thinking is
        # enabled -- a judge-style call with a small max_tokens can come
        # back with zero visible output, all of it spent on thinking.
        "max_tokens": max_tokens,
        "temperature": VOSS_TEMPERATURE if temperature is None else temperature,
        "messages": [{"role": "system", "content": system_prompt}] + messages,
    }
    # glm-5.3 (and, unverified so far, presumably glm-5.3-flash too) --
    # per docs.z.ai -- no longer accepts thinking.type="disabled" at all
    # ("an error will occur").
    # Checked live: the /coding/ subscription endpoint this app uses still
    # tolerates it today, but that's undocumented leniency, not a contract --
    # Z.ai's own migration guidance is enabled + reasoning_effort="low" as
    # the equivalent fast/cheap mode, so callers asking for "disabled" get
    # translated to that instead of the raw (increasingly unsafe) value.
    if thinking == "disabled":
        body_obj["thinking"] = {"type": "enabled"}
        body_obj["reasoning_effort"] = "low"
    else:
        body_obj["thinking"] = {"type": thinking}
    # Opt-in, not default -- only callers that actually want JSON back
    # (judge, resume-extract) should set this; Voss's conversational replies
    # are plain text. Z.ai supports this directly (response_format:
    # {"type": "json_object"}) instead of relying on "reply with ONLY a
    # JSON object" prompt wording plus a regex-extraction fallback.
    if response_format:
        body_obj["response_format"] = {"type": response_format}
    body = json.dumps(body_obj).encode()
    req = urllib.request.Request(
        ZAI_URL, data=body, method="POST",
        headers={"Authorization": f"Bearer {ZAI_API_KEY}", "content-type": "application/json"})
    delay = 1.0
    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                data = json.loads(resp.read())
            return data["choices"][0]["message"]["content"].strip()
        except urllib.error.HTTPError as e:
            body_text = e.read().decode(errors="replace")
            if e.code == 429 and attempt < 2:   # rate limit -- back off and retry, doubling each time
                print(f"[voss] Z.ai 429, retrying in {delay:.0f}s: {body_text[:200]}", file=sys.stderr)
                time.sleep(delay)
                delay *= 2
                continue
            print(f"[voss] Z.ai API error {e.code}: {body_text}", file=sys.stderr)
            return None
        except Exception as e:
            print(f"[voss] Z.ai API call failed: {e}", file=sys.stderr)
            return None
    return None


def _llm_call(system_prompt, messages, max_tokens=1200, temperature=None):
    if LLM_PROVIDER == "zai":
        return _zai_call(system_prompt, messages, max_tokens, temperature)
    return _anthropic_call(system_prompt, messages, max_tokens, temperature)


# Fallback mode for the structured JSON calls below (judge/draft/rewrite/
# extract): Zai is primary -- cheaper and it's what every one of those
# prompts was tuned against -- but Zai's own usage cap is real (hit live:
# "Usage limit reached for 5 hour" on a real 429). Without a fallback that
# just stops a judge/draft batch mid-run for hours. _llm_json_call is the
# ONE place every one of those call sites routes through: Zai first, Claude
# on any Zai failure, same (system_prompt, messages) shape either way, so
# no call site branches on provider itself -- one place to add a third
# provider, change the cooldown, or flip which one is primary.
#
# A short cooldown after a Zai failure skips straight to Claude for a while
# instead of re-paying Zai's own ~3s retry/backoff (see _zai_call) on every
# single call in a batch of dozens -- then tries Zai again once it lapses,
# so service comes back on its own without parsing/guessing Zai's exact
# reset time.
_ZAI_COOLDOWN_SECONDS = 300
_zai_cooldown_until = 0.0


def _llm_json_call(system_prompt, messages, max_tokens=1200, temperature=0.3, thinking="disabled",
                    zai_model=None, claude_model=None):
    """One JSON-shaped LLM call, routed to whichever provider is actually
    up: Zai (Flash by default, cheap/fast, primary) -> a real Anthropic API
    key if one's ever configured (ANTHROPIC_API_KEY) -> the Claude CLI on
    an OAuth subscription token (CLAUDE_CODE_OAUTH_TOKEN) as the true no-
    credits-required last resort. Returns (reply_text, provider) where
    provider is "zai", "claude-api", or "claude-cli", or (None, None) if
    every configured tier failed (or none are configured at all).

    zai_model overrides ZAI_MODEL (the Flash default); claude_model
    overrides ANTHROPIC_MODEL_FALLBACK (the Haiku default) on BOTH Claude
    branches -- each used by the one call site that wants the full,
    highest-stakes model instead of the cheap default for every provider
    at once, one kwarg per provider rather than a separate code path.

    Every caller's system_prompt must already say "reply with ONLY a JSON
    object" in its own words, and every caller already pulls the {...} out
    with a tolerant regex before json.loads -- that was already true for
    the Zai-only call sites this replaces, and it's exactly what makes
    neither Claude branch need anything extra: neither has Zai's native
    response_format JSON mode in this raw wrapper, but the same prompt
    wording + tolerant parse the Zai path always relied on carries both
    just as reliably."""
    global _zai_cooldown_until
    if time.time() >= _zai_cooldown_until:
        reply = _zai_call(system_prompt, messages, max_tokens=max_tokens, temperature=temperature,
                           thinking=thinking, response_format="json_object", model=zai_model)
        if reply is not None:
            return reply, "zai"
        _zai_cooldown_until = time.time() + _ZAI_COOLDOWN_SECONDS
        print(f"[llm] Zai call failed -- falling back to Claude, will retry Zai again in "
              f"{_ZAI_COOLDOWN_SECONDS}s", file=sys.stderr)
    reply = _anthropic_call(system_prompt, messages, max_tokens=max_tokens, temperature=temperature,
                             model=claude_model or ANTHROPIC_MODEL_FALLBACK)
    if reply is not None:
        return reply, "claude-api"
    reply = _claude_cli_call(system_prompt, messages, max_tokens=max_tokens, temperature=temperature,
                              model=claude_model or ANTHROPIC_MODEL_FALLBACK)
    return (reply, "claude-cli") if reply is not None else (None, None)


# Primary job-fit judgment (Zai-or-Claude via _llm_json_call above, not the LLM_PROVIDER switch
# above, since this is a separate concern from the interview persona). Gated
# by title + location alone (is_likely_engineering_title, non-manager tier,
# India/Bangalore/Bengaluru -- the same classification the Postings list
# itself filters on) -- NOT by the regex score, and NOT by the regex
# years-of-experience extraction either. Both were tried and dropped
# after a real 40-posting validation: regex score correlated only 0.28 with
# an independent LLM judgment on the same postings, and 40% of postings the
# regex would have scored below 50 were rated a real fit (>=70) by the LLM
# -- keyword matching can't bridge vocabulary (a "Database Engine" posting
# scored 21 on regex, 62 from the LLM, which correctly read the underlying
# distributed-systems background as transferable even though none of its
# specific terms literally matched). Years-of-experience is read directly
# out of the JD text by the LLM itself, not pre-filtered by a rigid regex
# number -- the LLM weighed a "4.5 vs 5+ years" gap proportionately against
# offsetting strengths; the regex penalty just subtracts a flat amount.
# Also hard-capped per fetch so a big backlog can't run away on cost.
JOBS_LLM_MAX_PER_FETCH = 25         # hard cap on calls per Fetch & score run
JOBS_JD_CHAR_LIMIT = 12000          # covers all but the single longest fetched posting in full


def _trim_jd(text, limit=JOBS_JD_CHAR_LIMIT):
    """Truncate at a real word/line boundary, not mid-word. A raw [:limit]
    slice was cutting real postings apart mid-word ("Prod|uct Security") --
    caught by checking what actually sat right at the cut point on the
    longest fetched posting, not assumed safe just because it compiled."""
    if len(text) <= limit:
        return text
    cut = text[:limit]
    boundary = max(cut.rfind(" "), cut.rfind("\n"))
    if boundary > limit * 0.9:   # don't backtrack far enough to lose a lot
        cut = cut[:boundary]
    return cut + "\n\n[...truncated]"


def _zai_judge_job(job, profile, points_text, skill_names, candidate_years):
    """Returns (score, reason) or (None, None) if no key is set, or the
    call/parse fails -- callers treat that exactly like "not judged yet",
    never as a 0.

    The rubric below is ats_scorer_spec.md operationalized as prompt
    instructions, not a vague "judge fit": for each real requirement, the
    model has to reason about presence (incl. semantic equivalence --
    the documented #1 cause of real false-negative ATS rejections, which
    regex can't judge at all), placement (title/summary/bullet/skills-list-
    only), depth (a quantified outcome vs a bare name-drop), and recency
    (a skill from a recent, sustained role outweighs one from a brief old
    stint). Asking for the per-requirement list in the JSON response forces
    that reasoning to actually happen -- a bare score+sentence request lets
    the model skip straight to a vibe.

    Two fixes found by checking a real judgment (Okta job 7460) against the
    real posting text: the reason was naming a "Nice to have" gap (IAM
    protocols) as if it were as core as the actual "Required" gap (AD/LDAP)
    sitting right next to it in the same sentence -- the posting itself
    distinguishes required from nice-to-have, the model wasn't being asked
    to. And the reason named the gap but never said what would help close
    it -- useful for a yes/no read, not for deciding what to do next.
    `importance` and the second reason sentence below are the fix for each."""
    system = (
        'You score how well a real candidate fits one job posting, against a specific rubric -- '
        'not a vague impression. Identify the 5 to 8 requirements from the posting that actually '
        'matter (hard skills, tools, years of experience, domain knowledge), then for EACH one judge:\n'
        '- importance: "required" (posting lists it under Required/Must-have, or states it as a hard '
        'bar) or "nice_to_have" (posting lists it under Nice-to-have/Preferred/Bonus, or implies it\'s '
        'not a hard bar). If the posting doesn\'t label sections, infer from phrasing ("must", "required" '
        'vs "a plus", "bonus").\n'
        '- presence: "yes" (named or demonstrated), "semantic" (a different but equivalent term or '
        'clearly-transferable work -- e.g. JD wants "React", candidate shows "React.js"; JD wants '
        '"Kubernetes", candidate ran "container orchestration at scale" using it), or "no". Never mark '
        '"semantic" for something only vaguely related -- that is still "no".\n'
        '- placement: "title_summary", "bullet", "skills_list_only", or "n/a" if presence is "no".\n'
        '- depth: "quantified" (a real number, scale, or named outcome backs it), "named" (mentioned '
        'with no quantified backing), or "n/a" if presence is "no".\n'
        '- recency: "recent" (current or last ~2 years, sustained), "dated" (older or a brief stint), '
        'or "n/a" if presence is "no".\n'
        'Then give an overall score, honest and specific -- weigh a missing "required" item far more '
        'than a missing "nice_to_have" one, and never inflate a "no" into a "semantic" to be generous. '
        'Reply with ONLY a JSON object: {"key_requirements": [{"requirement": str, "importance": str, '
        '"presence": str, "placement": str, "depth": str, "recency": str}], "score": <0-100 int>, '
        '"reason": "<two sentences. First: name the real gap if there is one -- say plainly whether it\'s '
        'a required or nice-to-have gap, don\'t blur the two together -- or the strongest match if there '
        'isn\'t a real gap. Second: one concrete, specific thing that would actually close that gap or '
        'strengthen that match (not generic career advice) -- if there is no real gap, say what to keep '
        'leading with instead.>"}.'
    )
    user = (
        f"Candidate: {profile.get('title', '')}, {candidate_years} years of real experience.\n\n"
        f"Real accomplishments, with company/role/dates (use the dates for the recency judgment):\n"
        f"{points_text}\n\n"
        f"Known skills (a flat list -- counts as skills_list_only placement unless the same skill also "
        f"appears in a bullet above): {', '.join(skill_names)}\n\n"
        f"Job posting -- {job['title']} at {job['company']}:\n{_trim_jd(job['description_raw'])}"
    )
    # thinking="disabled" -> translated to low-effort thinking inside
    # _zai_call (glm-5.3 doesn't accept a hard off switch any more) -- still
    # the fast/cheap mode, not full deep reasoning. response_format forces
    # real JSON back instead of trusting "reply with ONLY a JSON object"
    # prompt wording alone. max_tokens raised from 500, then again from
    # 1200: the structured list carries one more field per requirement
    # (importance) and reason is two sentences, not one -- real extra
    # output, not just a longer ask.
    reply, provider = _llm_json_call(system, [{"role": "user", "content": user}], max_tokens=1500, temperature=0.2)
    if reply is None:
        return None, None
    try:
        m = re.search(r"\{.*\}", reply, re.S)   # tolerate stray text/fences around the JSON
        obj = json.loads(m.group(0) if m else reply)
        # The two-sentence reason (gap + actionable fix) naturally runs
        # ~500-550 chars -- a flat [:500] slice was chopping the actionable
        # second sentence off mid-word nearly every time (caught by actually
        # measuring raw reason length across real postings, not assumed
        # safe). 700 clears that with real headroom; still word-boundary
        # safe like _trim_jd, in case a reply ever runs long.
        reason = str(obj.get("reason", ""))
        if len(reason) > 700:
            cut = reason[:700]
            boundary = max(cut.rfind(" "), cut.rfind("\n"))
            reason = cut[:boundary] if boundary > 700 * 0.9 else cut
        return int(obj["score"]), reason
    except Exception as e:
        print(f"[jobs] {provider} judgment parse failed: {e} -- raw: {reply[:200]}", file=sys.stderr)
        return None, None


def _zai_draft_application(job, profile, points_text, skill_names, candidate_years):
    """Writes a resume summary + cover letter body for one job, grounded
    strictly in the real point bank -- never invents a fact, number, or
    accomplishment. Returns (summary, cover_letter_body, None) on success,
    or (None, None, reason) on failure -- reason distinguishes "the Zai
    call itself failed" (no key, rate-limited, network) from "it replied
    but never passed the humanizer gate in 3 tries", since those need
    different responses (wait and retry vs. look at what it kept writing).
    An earlier version collapsed both into one vague message -- caught
    immediately on the first real batch run: all 3 jobs "failed the
    humanizer gate", which was actually a Zai 429 the whole time.

    Resolves the tension the original hand-written-only design existed to
    avoid: a blind LLM call producing generic "I am excited to apply..."
    AI-slop. The fix isn't "no LLM", it's a real enforced gate -- every
    draft is checked with humanizer.check_human_style() (the same hard
    gate generate() itself refuses to render past) before it's accepted,
    with up to 2 retries feeding the exact violations back if it fails.
    Grounding is enforced the same way the resume-extraction prompt
    already does it elsewhere in this file: explicit "never invent"
    instruction plus real point-bank text as the only source of facts."""
    system = (
        "You write a tailored resume summary and cover letter for one real job posting, "
        "grounded ONLY in the candidate's real accomplishments given below. Never invent a "
        "fact, number, project, or outcome not literally present in the point bank text -- "
        "if the posting wants something the point bank doesn't cover, don't claim it, just "
        "don't mention it. Be specific: reference real numbers and real systems from the "
        "point bank, and real specifics from the posting (the team, the problem, a named "
        "technology) -- not generic enthusiasm.\n\n"
        + HUMAN_STYLE_RULES +
        "\nResume summary: 1-2 sentences, no greeting, states the candidate's real "
        "background and seniority plainly.\n"
        "Cover letter body: 3-4 short paragraphs, no \"Dear Hiring Manager\" greeting or "
        "sign-off (the renderer adds those), grounded in specific real point-bank facts "
        "connected to specific things this posting actually asks for.\n\n"
        "Reply with ONLY a JSON object: {\"summary\": str, \"cover_letter_body\": str} "
        "(cover_letter_body paragraphs separated by a blank line, i.e. \\n\\n)."
    )
    user = (
        f"Candidate: {profile.get('title', '')}, {candidate_years} years of real experience.\n\n"
        f"Real accomplishments, with company/role/dates:\n{points_text}\n\n"
        f"Known skills: {', '.join(skill_names)}\n\n"
        f"Job posting -- {job['title']} at {job['company']}:\n{_trim_jd(job['description_raw'])}"
    )
    messages = [{"role": "user", "content": user}]
    for attempt in range(3):
        reply, provider = _llm_json_call(system, messages, max_tokens=1200, temperature=0.4)
        if reply is None:
            return None, None, "LLM call failed on both Zai and Claude -- see server logs (often a rate limit; check for a 429)"
        try:
            m = re.search(r"\{.*\}", reply, re.S)
            obj = json.loads(m.group(0) if m else reply)
            summary, cover = str(obj["summary"]).strip(), str(obj["cover_letter_body"]).strip()
        except Exception as e:
            print(f"[jobs] {provider} draft parse failed: {e} -- raw: {reply[:200]}", file=sys.stderr)
            return None, None, f"{provider} reply wasn't valid JSON: {e}"
        violations = (jobs_humanizer.check_human_style(summary)
                      + jobs_humanizer.check_human_style(cover))
        if not violations:
            return summary, cover, None
        if attempt < 2:
            messages.append({"role": "assistant", "content": reply})
            messages.append({"role": "user", "content": (
                "That draft fails the style check -- fix these and reply with the same "
                "JSON shape, nothing else:\n- " + "\n- ".join(violations))})
    reason = "failed the humanizer gate 3x in a row: " + "; ".join(violations)
    print(f"[jobs] {provider} draft for job {job['id']} {reason}", file=sys.stderr)
    return None, None, reason


def _zai_rewrite_point(raw_text, company, role):
    """Reformats ONE point-bank bullet into Keyword+Use+Result shape --
    never invents. Same "never invent a fact... if ambiguous, leave it out"
    discipline the resume-upload extraction prompt already uses, applied
    to a single bullet instead of a whole resume: if a real brag/result is
    already present just phrased badly, rewrite it; if it genuinely isn't
    there, refuse and say exactly what's missing instead of making one up.
    This is the real master-doc-time enforcement of the rubric, replacing
    a bullet that only ever got flagged with one that actually gets fixed.

    Returns a dict: {"status": "rewritten", "text": str} or
    {"status": "missing_info", "explanation": str, "question": str}, or
    None if the Zai call itself failed (caller should treat that as "try
    again later", not as a real refusal)."""
    system = (
        "You reformat ONE real resume bullet point into a stronger shape, without ever "
        "inventing anything. Never add a fact, number, outcome, or detail that isn't "
        "literally present in the text given to you -- if you're tempted to guess a number "
        "or soften a gap with a vague phrase, stop and refuse instead.\n\n"
        "The target shape: name the real skill/technology, how it was actually used, and "
        "the measurable result or named outcome -- one bullet, one or two sentences, no "
        "paragraph.\n\n"
        "If the given text already contains a real result (a number, a named outcome, an "
        "award, concrete feedback) but is phrased awkwardly or buries it, rewrite it into "
        "that shape, preserving every real fact and number exactly as given.\n\n"
        "If the given text has NO real result or outcome anywhere in it -- a bare activity "
        "or responsibility with nothing measurable or named -- do NOT invent one. Instead "
        "explain in one sentence what's missing, then ask ONE specific, concrete question "
        "that would help recall the real missing fact (a number, a before/after, who "
        "noticed, what changed).\n\n"
        "No AI-sounding buzzwords (delve, leverage, underscore, boast, robust, seamless), "
        "no em dashes.\n\n"
        "Reply with ONLY a JSON object, one of:\n"
        '{"status": "rewritten", "text": "<the rewritten bullet>"}\n'
        '{"status": "missing_info", "explanation": "<one sentence, what\'s missing>", '
        '"question": "<one specific question to help recall it>"}'
    )
    user = f"Company/role: {company} / {role}\n\nBullet as written:\n{raw_text}"
    # Full model on every provider, not the cheap default -- this is the one
    # call site writing into the master document itself, where the "never
    # invent" discipline has to hold up under real scrutiny, not just the
    # cheap/high-volume judge and draft calls elsewhere.
    reply, provider = _llm_json_call(system, [{"role": "user", "content": user}], max_tokens=500, temperature=0.3,
                                      zai_model=ZAI_MODEL_FULL, claude_model=ANTHROPIC_MODEL_FULL)
    if reply is None:
        return None
    try:
        m = re.search(r"\{.*\}", reply, re.S)
        obj = json.loads(m.group(0) if m else reply)
        if obj.get("status") == "rewritten":
            return {"status": "rewritten", "text": str(obj["text"]).strip()}
        return {"status": "missing_info", "explanation": str(obj.get("explanation", "")).strip(),
                "question": str(obj.get("question", "")).strip()}
    except Exception as e:
        print(f"[jobs] {provider} point-rewrite parse failed: {e} -- raw: {reply[:200]}", file=sys.stderr)
        return None


# AI-judge, async: a real batch over "all" stale/new candidates is however
# many real LLM calls that is (seconds each) -- blocking the HTTP request
# for that risked a timeout the moment the candidate set got large, now that
# there's no artificial per-call cap. Same shape as Voss's own async
# pattern above (a daemon thread, one lock so only one batch runs app-wide,
# real progress read by polling, not by holding the request open).
_judge_lock = threading.Lock()
_judge_progress = {"running": False, "total": 0, "done": 0, "judged": 0, "started_ts": None}


def _run_judge_batch(candidates, profile, points_text, skill_names, candidate_years):
    """Returns False without starting anything if a batch is already
    running -- one at a time app-wide, same as Voss's one-interview-at-a-
    time rule, so two clicks don't double-spend on the same candidates."""
    if _judge_progress["running"]:
        return False
    _judge_progress.update({"running": True, "total": len(candidates), "done": 0,
                             "judged": 0, "started_ts": time.time()})

    def run():
        with _judge_lock:
            for j in candidates:
                fresh = jobsdb.jobs_get(j["id"])
                if fresh is not None:
                    score, reason = _zai_judge_job(fresh, profile, points_text, skill_names,
                                                    candidate_years)
                    if score is not None:
                        jobsdb.jobs_set_llm_score(fresh["id"], score, reason)
                        _judge_progress["judged"] += 1
                _judge_progress["done"] += 1
            _judge_progress["running"] = False
    threading.Thread(target=run, daemon=True).start()
    return True


# Generate, async -- same pattern as the judge batch above. "Queue for
# resume + cover letter" used to only set a status flag with nothing
# watching it; the only way a draft ever got written was asking Claude
# directly, per job, in chat -- which is why queuing a job and waiting
# showed no progress at all, there was nothing to show progress ON. This
# is the real automated path: Zai drafts the summary + cover letter
# (_zai_draft_application, grounded in the real point bank, gated by the
# same humanizer check generate() itself enforces), then the existing
# generate_cli.py pipeline renders it exactly like a Claude-authored draft
# would -- same one-page enforcement, same Keyword+Use+Result bullet
# filter, same PDF export. One quality bar regardless of who wrote the
# prose.
_generate_lock = threading.Lock()
_generate_progress = {"running": False, "total": 0, "done": 0, "generated": 0,
                       "failed": [], "started_ts": None}


def _generate_one(job_id, summary, cover_letter_body):
    """Shells into the jobsearch venv the same way the manual
    /api/jobs/generate handler does -- one real code path for "how a
    resume gets rendered" regardless of whether Claude or Zai wrote the
    prose. Returns (ok, error_or_None)."""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump({"job_id": job_id, "summary": summary, "cover_letter_body": cover_letter_body}, f)
        req_path = f.name
    try:
        out = subprocess.run(
            [JOBSEARCH_VENV_PY, os.path.join(JOBSEARCH_DIR, "generate_cli.py"), req_path],
            capture_output=True, text=True, timeout=60, cwd=JOBSEARCH_DIR,
        )
    finally:
        os.unlink(req_path)
    if out.returncode != 0:
        return False, (out.stderr or out.stdout)[-500:]
    try:
        result = json.loads(out.stdout.strip().splitlines()[-1])
    except Exception:
        return False, f"bad generate_cli output: {out.stdout[-300:]} {out.stderr[-300:]}"
    if "error" in result:
        return False, result["error"][:500]
    return True, None


def _run_generate_batch(candidates, profile, points_text, skill_names, candidate_years):
    """candidates: queued jobs. Returns False without starting anything if
    a batch is already running -- same one-at-a-time rule as the judge
    batch, so two clicks don't double-draft the same jobs."""
    if _generate_progress["running"]:
        return False
    _generate_progress.update({"running": True, "total": len(candidates), "done": 0,
                                "generated": 0, "failed": [], "started_ts": time.time()})

    def run():
        with _generate_lock:
            for j in candidates:
                fresh = jobsdb.jobs_get(j["id"])
                if fresh is not None and fresh["status"] == "queued":
                    summary, cover, draft_err = _zai_draft_application(
                        fresh, profile, points_text, skill_names, candidate_years)
                    if summary is not None:
                        ok, err = _generate_one(fresh["id"], summary, cover)
                        if ok:
                            _generate_progress["generated"] += 1
                        else:
                            _generate_progress["failed"].append(
                                {"job_id": fresh["id"], "title": fresh["title"],
                                 "company": fresh["company"], "reason": err})
                    else:
                        _generate_progress["failed"].append(
                            {"job_id": fresh["id"], "title": fresh["title"],
                             "company": fresh["company"], "reason": draft_err})
                _generate_progress["done"] += 1
            _generate_progress["running"] = False
    threading.Thread(target=run, daemon=True).start()
    return True


# Fetch, async -- same pattern as the judge batch above. This used to block
# the POST /api/jobs/refetch request itself on subprocess.run() for up to
# 120s with no progress feedback at all (the browser's "Fetch new" click
# just hung); a real run across the full company list takes well over a
# minute. Now it starts a background thread and returns immediately; the UI
# polls /api/jobs/refetch/status the same way it already polls AI-judge.
_fetch_lock = threading.Lock()
_fetch_progress = {"running": False, "started_ts": None, "ok": None, "new_postings": None, "log": ""}


def _run_fetch_async():
    """Returns False without starting anything if a fetch is already
    running -- one at a time, same "no double-spend / no double-run"
    reasoning as the judge batch."""
    if _fetch_progress["running"]:
        return False
    _fetch_progress.update({"running": True, "started_ts": time.time(), "ok": None,
                             "new_postings": None, "log": ""})

    def run():
        with _fetch_lock:
            before = {j["id"] for j in jobsdb.jobs_list()}
            out = subprocess.run([JOBSEARCH_VENV_PY, "fetch_ats.py"], capture_output=True,
                                  text=True, timeout=120, cwd=JOBSEARCH_DIR)
            after_jobs = jobsdb.jobs_list()
            new_job_ids = {j["id"] for j in after_jobs if j["id"] not in before}
            _fetch_progress.update({
                "ok": out.returncode == 0, "new_postings": len(new_job_ids),
                "log": out.stdout[-4000:] + out.stderr[-2000:], "running": False,
            })
    threading.Thread(target=run, daemon=True).start()
    return True


def _extract_file_text(path, ext):
    """PDF via pdftotext (system-wide, already proven elsewhere in this
    repo), docx via jobsearch's own venv (python-docx isn't installed for
    this process's plain python3). Returns "" on failure rather than
    raising -- an unreadable upload should surface as "found nothing",
    not crash the request."""
    try:
        if ext == ".pdf":
            out = subprocess.run(["pdftotext", "-layout", path, "-"], capture_output=True, text=True, timeout=30)
            return out.stdout
        if ext == ".docx":
            code = ("import sys\nfrom docx import Document\n"
                    "d = Document(sys.argv[1])\n"
                    "print('\\n'.join(p.text for p in d.paragraphs))")
            out = subprocess.run([JOBSEARCH_VENV_PY, "-c", code, path],
                                  capture_output=True, text=True, timeout=30)
            return out.stdout
        if ext in (".txt", ".md"):
            with open(path, "r", errors="replace") as f:
                return f.read()
    except Exception as e:
        print(f"[profile] extract failed: {e}", file=sys.stderr)
    return ""


def _zai_extract_profile(resume_text, known_companies, existing_points_text):
    """Structured extraction: real resume text in, candidate point_bank
    rows + skills out -- grounded, never invents beyond what's literally in
    the text. Caller treats the result as a PROPOSAL for human review, never
    auto-committed, same discipline as everywhere else in this project.

    existing_points_text is shown to the model so it can skip proposing a
    point that just reworded something already on file -- resume variants
    kept re-extracting the same accomplishment with slightly different
    phrasing and it was landing in the point bank twice with no check at
    all. This catches the paraphrased case an exact/fuzzy string match on
    the *output* can miss; point_bank_find_similar (difflib, in db.py) is
    the deterministic safety net for near-verbatim repeats specifically."""
    system = (
        'Extract structured career facts from a resume. Rules: (1) never invent a fact, number, or skill '
        'not literally stated in the text -- if something is ambiguous, leave it out rather than guess. '
        '(2) One point_bank row per distinct accomplishment, verbatim or lightly cleaned up from the '
        'source text, not rewritten or embellished. (3) Reuse company names exactly as they already appear '
        'in the candidate\'s known companies list if the resume clearly refers to the same employer. '
        '(4) Do NOT propose a point that describes the same underlying accomplishment as one already listed '
        'below, even if the wording differs -- skip it entirely rather than re-adding a reworded duplicate. '
        'Only propose points for facts/numbers not already covered.\n'
        'Reply with ONLY a JSON object: {"points": [{"company": str, "role": str, "start_date": '
        '"YYYY-MM or empty", "end_date": "YYYY-MM or empty (empty means current)", "text": str, '
        '"tags": [str, ...]}], "skills": [{"name": str, "category": str}]}.'
    )
    user = (f"Known companies already on file: {', '.join(known_companies) or '(none yet)'}\n\n"
            f"Accomplishments already in the point bank -- do not re-propose any of these, reworded or not:\n"
            f"{existing_points_text or '(none yet)'}\n\n"
            f"Resume text:\n{resume_text[:15000]}")
    reply, provider = _llm_json_call(system, [{"role": "user", "content": user}], max_tokens=4000, temperature=0.1)
    if reply is None:
        return None, "no LLM provider configured, or both Zai and Claude calls failed -- check server logs"
    try:
        m = re.search(r"\{.*\}", reply, re.S)
        obj = json.loads(m.group(0) if m else reply)
        return obj, None
    except Exception as e:
        return None, f"couldn't parse the model's response: {e}"


_voss_lock = threading.Lock()   # one interview at a time app-wide -- one lock is enough


def voss_open(iid, qid, part):
    def run():
        with _voss_lock:
            iv = db.interview_get(iid)
            if not iv or iv["status"] != "active":
                return
            msgs = [{"role": "user", "content":
                     "(The candidate has just joined. Open the interview: a brief line "
                     "of logistics, then state the problem and ask them to walk through "
                     "their approach before writing any code.)"}]
            reply = _llm_call(_system_prompt(qid, part, iv["code"]), msgs)
            if reply:
                db.interview_add_message(iid, "interviewer", reply)
    threading.Thread(target=run, daemon=True).start()


def voss_reply(iid, qid, part):
    def run():
        with _voss_lock:
            iv = db.interview_get(iid)
            if not iv or iv["status"] != "active":
                return
            reply = _llm_call(_system_prompt(qid, part, iv["code"]), _transcript_messages(iid))
            if reply:
                db.interview_add_message(iid, "interviewer", reply)
    threading.Thread(target=run, daemon=True).start()


def voss_review(iid, qid, part):
    def run():
        with _voss_lock:
            iv = db.interview_get(iid)
            if not iv:
                return
            q = find_question(qid)
            verdict_line = "(could not evaluate the final code)"
            if q:
                pdir = os.path.join(q["dir"], "parts", str(part))
                try:
                    ev = java_runner.evaluate if q.get("lang") == "java" else runner.evaluate
                    res = ev(iv["code"] or "", pdir, quick=False)
                    verdict_line = f"Automated verdict: {res['verdict']}" + (
                        f" -- {res.get('reason')}" if res.get("reason") else "")
                except Exception as e:
                    verdict_line = f"(evaluation failed: {e})"
            close_instr = {
                "role": "user",
                "content": (
                    "(Time is up / the candidate ended the session. Write the closing "
                    "review now, in markdown, with exactly these sections: **What "
                    "happened** (one short paragraph, and include this real result "
                    f"verbatim: {verdict_line}), **Strengths** (bulleted, specific), "
                    "**Gaps / things to think about** (bulleted, specific), **Verdict** "
                    "(one calibrated line). Do not add any other sections.)"
                ),
            }
            review = _llm_call(_system_prompt(qid, part, iv["code"]),
                                _transcript_messages(iid, extra=close_instr), max_tokens=1800)
            if review:
                db.interview_set_review(iid, review)
    threading.Thread(target=run, daemon=True).start()


# ── judged questions: no compiler, an LLM grades one written answer ───────
# For a `kind: "judged"` question there's no boilerplate/solution/tests -- the
# candidate writes a plain-text answer in the same editor, Submit sends it here
# instead of to runner.evaluate(), and the verdict comes back synchronously
# (this is a single request/response call, not a background thread like Voss).
def judge_answer(q, part, answer, push):
    pdir = os.path.join(q["dir"], "parts", str(part))
    statement = _read(os.path.join(pdir, "statement.md"))
    rubric = _read(os.path.join(pdir, "rubric.md"))
    if not answer.strip():
        push({"stage": "judge", "status": "skip", "detail": "nothing written"})
        return {"verdict": "REJECTED", "reason": "no answer written", "stages": ["judge"]}
    system_prompt = judge_persona(load_pack(q["topic"]))
    context = (f"## The question\n\n{statement}\n\n"
               f"## What a good answer covers\n\n{rubric or '(no rubric on file -- judge on general soundness)'}\n\n"
               f"## The candidate's answer\n\n{answer}")
    verdict_text = _llm_call(system_prompt, [{"role": "user", "content": context}], max_tokens=700)
    if verdict_text is None:
        msg = "No LLM provider is configured yet -- can't judge this automatically."
        push({"stage": "judge", "status": "fail", "detail": msg})
        return {"verdict": "REJECTED", "reason": "no judge available", "stages": ["judge"]}
    first_line, _, rest = verdict_text.partition("\n")
    passed = first_line.strip().upper().startswith("PASS")
    feedback = rest.strip() or verdict_text
    push({"stage": "judge", "status": "ok" if passed else "fail", "detail": feedback})
    return {"verdict": "CLEAN" if passed else "REJECTED", "reason": feedback, "stages": ["judge"]}


# ── WebSocket (just enough of RFC 6455 for a local terminal) ─────────────
WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def ws_accept(key):
    return base64.b64encode(hashlib.sha1((key + WS_MAGIC).encode()).digest()).decode()


def ws_frame(payload, opcode=0x1):
    if isinstance(payload, str):
        payload = payload.encode("utf-8", "replace")
    n = len(payload)
    head = bytes([0x80 | opcode])
    if n < 126:
        head += bytes([n])
    elif n < (1 << 16):
        head += bytes([126]) + struct.pack(">H", n)
    else:
        head += bytes([127]) + struct.pack(">Q", n)
    return head + payload


def ws_read(sock):
    """Return (opcode, payload) or (None, None) when the peer goes away."""
    def recvn(n):
        buf = b""
        while len(buf) < n:
            chunk = sock.recv(n - len(buf))
            if not chunk:
                return None
            buf += chunk
        return buf

    hdr = recvn(2)
    if not hdr:
        return None, None
    opcode = hdr[0] & 0x0F
    masked = hdr[1] & 0x80
    ln = hdr[1] & 0x7F
    if ln == 126:
        ext = recvn(2)
        if not ext:
            return None, None
        ln = struct.unpack(">H", ext)[0]
    elif ln == 127:
        ext = recvn(8)
        if not ext:
            return None, None
        ln = struct.unpack(">Q", ext)[0]
    mask = recvn(4) if masked else b"\0\0\0\0"
    if mask is None:
        return None, None
    data = recvn(ln) if ln else b""
    if data is None:
        return None, None
    if masked:
        data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
    return opcode, data


def serve_terminal(sock):
    """Fork a shell on a pty and relay it over the socket."""
    shell = os.environ.get("SHELL", "/bin/zsh")
    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(REPO)
        os.environ["PS1"] = "prep$ "
        os.execvp(shell, [shell])
        os._exit(1)

    alive = threading.Event()
    alive.set()

    def pump_out():
        while alive.is_set():
            try:
                r, _, _ = select.select([fd], [], [], 0.2)
                if fd in r:
                    data = os.read(fd, 65536)
                    if not data:
                        break
                    sock.sendall(ws_frame(data.decode("utf-8", "replace")))
            except OSError:
                break
        alive.clear()

    t = threading.Thread(target=pump_out, daemon=True)
    t.start()
    try:
        while alive.is_set():
            op, data = ws_read(sock)
            if op is None or op == 0x8:
                break
            if op == 0x1:
                txt = data.decode("utf-8", "replace")
                if txt.startswith("\x00resize:"):      # {cols},{rows}
                    try:
                        cols, rows = (int(x) for x in txt[8:].split(","))
                        import fcntl, termios
                        fcntl.ioctl(fd, termios.TIOCSWINSZ,
                                    struct.pack("HHHH", rows, cols, 0, 0))
                    except Exception:
                        pass
                else:
                    os.write(fd, txt.encode())
    except OSError:
        pass
    finally:
        alive.clear()
        try:
            os.kill(pid, signal.SIGKILL)
            os.waitpid(pid, 0)
        except OSError:
            pass
        try:
            os.close(fd)
        except OSError:
            pass


# ── HTTP ─────────────────────────────────────────────────────────────────
class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    # -- helpers
    def _json(self, obj, code=200):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _body(self):
        n = int(self.headers.get("Content-Length", "0") or 0)
        return json.loads(self.rfile.read(n) or b"{}")

    def _static(self, path, root=STATIC):
        rel = path.lstrip("/") or "index.html"
        full = os.path.normpath(os.path.join(root, rel))
        if not full.startswith(root) or not os.path.isfile(full):
            self.send_error(404)
            return
        ctype = {".html": "text/html", ".js": "application/javascript",
                 ".css": "text/css", ".json": "application/json"}.get(
            os.path.splitext(full)[1], "application/octet-stream")
        with open(full, "rb") as f:
            body = f.read()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _serve_application_file(self, rel):
        """Download a generated resume.docx / cover_letter.docx from
        jobsearch/applications/. Same normpath-containment check as
        _static, since rel comes straight from the URL."""
        full = os.path.normpath(os.path.join(JOBSEARCH_APPLICATIONS, rel))
        if not full.startswith(JOBSEARCH_APPLICATIONS) or not os.path.isfile(full):
            self.send_error(404)
            return
        ctype = {
            ".docx": "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
            ".pdf": "application/pdf",
            ".md": "text/markdown",
        }.get(os.path.splitext(full)[1], "application/octet-stream")
        with open(full, "rb") as f:
            body = f.read()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Content-Disposition", f'attachment; filename="{os.path.basename(full)}"')
        self.end_headers()
        self.wfile.write(body)

    def _find(self, qid):
        return find_question(qid)

    # -- routing
    def do_GET(self):
        u = urlparse(self.path)
        p, qs = u.path, parse_qs(u.query)

        if p == "/ws/term" and self.headers.get("Upgrade", "").lower() == "websocket":
            key = self.headers.get("Sec-WebSocket-Key", "")
            self.send_response(101)
            self.send_header("Upgrade", "websocket")
            self.send_header("Connection", "Upgrade")
            self.send_header("Sec-WebSocket-Accept", ws_accept(key))
            self.end_headers()
            self.wfile.flush()
            serve_terminal(self.connection)
            self.close_connection = True
            return

        if p == "/api/questions":
            qs_all = load_questions()
            st = db.all_status(qs_all)
            stars = db.stars_all()
            for q in qs_all:
                q.pop("dir", None)
                q["state"] = st[q["id"]]
                q["label"] = db.label(st[q["id"]]["base"]) if st[q["id"]]["base"] else "unsolved"
                q["starred"] = q["id"] in stars
            rollup = db.pack_rollup(qs_all)
            packs = {topic: {**counts, "name": load_pack(topic)["name"]}
                     for topic, counts in rollup.items()}
            return self._json({"questions": qs_all, "packs": packs})

        if p == "/api/question":
            q = self._find(qs.get("id", [""])[0])
            if not q:
                return self._json({"error": "no such question"}, 404)
            part = int(qs.get("part", ["0"])[0])
            db.log(q["id"], part, "open")
            return self._json(part_payload(q, part))

        if p == "/api/revision":
            return self._json({"rows": db.revision_queue(load_questions())})

        if p == "/api/interview/poll":
            iid = qs.get("id", [""])[0]
            since = int(qs.get("since", ["0"])[0])
            iv = db.interview_get(int(iid)) if iid else (db.interview_active() or db.interview_last())
            if not iv:
                return self._json({"interview": None, "messages": []})
            return self._json({"interview": iv, "messages": db.interview_messages_since(iv["id"], since)})

        if p == "/api/interview/history":
            rows = db.interview_history()
            for r in rows:
                q = find_question(r["qid"])
                r["title"] = q["title"] if q else r["qid"]
            return self._json({"interviews": rows})

        if p == "/api/jobs/list":
            total_unfiltered, matched = _filter_jobs(qs)
            page = max(1, int(qs.get("page", ["1"])[0]))
            per_page = max(1, min(100, int(qs.get("per_page", ["10"])[0])))
            applied_ids = {a["job_id"] for a in jobsdb.applications_all() if a["status"] != "generated"}
            start = (page - 1) * per_page
            page_jobs = [_job_public(j, applied_ids) for j in matched[start:start + per_page]]
            return self._json({"jobs": page_jobs, "total_matched": len(matched), "total_unfiltered": total_unfiltered,
                                "page": page, "per_page": per_page,
                                "total_pages": max(1, (len(matched) + per_page - 1) // per_page)})

        if p == "/api/jobs/companies":
            _, matched = _filter_jobs(qs)
            by_company = {}
            for j in matched:
                c = by_company.setdefault(j["company"], {"company": j["company"], "count": 0,
                                                           "best_score": None, "best_title": ""})
                c["count"] += 1
                eff = j["llm_score"]
                if eff is not None and (c["best_score"] is None or eff > c["best_score"]):
                    c["best_score"] = eff
                    c["best_title"] = j["title"]
            rows = sorted(by_company.values(),
                          key=lambda c: c["best_score"] if c["best_score"] is not None else -1, reverse=True)
            return self._json({"companies": rows})

        if p == "/api/jobs/llm-backfill/status":
            # Polled by the UI while a batch is running -- real progress
            # (judged so far / total), not a guess, since each score is
            # written to the db the moment that one call finishes.
            return self._json(dict(_judge_progress))

        if p == "/api/jobs/refetch/status":
            return self._json(dict(_fetch_progress))

        if p == "/api/jobs/generate-queued/status":
            return self._json(dict(_generate_progress))

        if p == "/api/jobs/detail":
            # No regex fit-score computed here anymore -- regex's job is
            # title/seniority/location classification only, per Sourav's
            # explicit instruction. The only real fit judgment is the LLM
            # one, already on the row (llm_score/llm_reason) if AI-judge has
            # run on this posting, null otherwise.
            jid = int(qs.get("id", ["0"])[0])
            j = jobsdb.jobs_get(jid)
            if not j:
                return self._json({"error": "no such job"}, 404)
            # Raw row, not _job_public() -- detail intentionally exposes more
            # (description_raw, content_hash, ...) than the list view does --
            # but "stale" is still real, computed state the panel needs, so
            # it's added here rather than silently missing from this one path.
            j["stale"] = j["llm_score"] is not None and jobsdb.jobs_llm_score_stale(j)
            return self._json({"job": j, "applications": jobsdb.applications_for_job(jid)})

        if p == "/api/applications/list":
            return self._json({"applications": jobsdb.applications_all()})

        if p == "/api/profile/points":
            show_all = qs.get("all", ["0"])[0] == "1"
            points = jobsdb.point_bank_all(active_only=not show_all)
            # Keyword+Use+Result rubric, authoring-time half (brag + no bare
            # generic-verb opener) -- surfaced on every real point here so a
            # weak one is visible at a glance in Manage Entries, not just
            # the moment you happen to touch it. A bracket-flagged draft
            # row is skipped -- it's not real resume content yet, nothing
            # to grade.
            for pt in points:
                pt["quality"] = (None if pt["text"].lstrip().startswith("[")
                                  else jobs_ats.check_bullet_brag(pt["text"]))
            return self._json({"points": points})

        if p == "/api/profile/skills":
            return self._json({"skills": jobsdb.skills_all()})

        if p == "/api/profile/fields":
            return self._json({"profile": jobsdb.profile_all()})

        if p == "/":
            self.send_response(302)
            self.send_header("Location", "/jobs/")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return

        if p == "/prepare" or p.startswith("/prepare/"):
            return self._static(p[len("/prepare"):] or "/", root=STATIC)

        if p == "/jobs" or p.startswith("/jobs/"):
            return self._static(p[len("/jobs"):] or "/", root=JOBSEARCH_STATIC)

        if p.startswith("/applications/"):
            return self._serve_application_file(p[len("/applications/"):])

        return self._static(p)

    # -- mock interview (its own routing: "id" here means interview id, not qid)
    def _interview_post(self, p, b):
        if p == "/api/interview/start":
            iv = db.interview_start(b.get("qid", ""), int(b.get("part", 0)),
                                     int(b.get("duration_s", 1800)))
            if iv["status"] == "active" and not db.interview_messages_since(iv["id"], 0):
                voss_open(iv["id"], iv["qid"], iv["part"])   # freshly created, not resumed
            return self._json({"interview": iv})

        iid = int(b.get("id", 0))

        if p == "/api/interview/message":
            db.interview_add_message(iid, "candidate", b.get("text", ""))
            iv = db.interview_get(iid)
            if iv and iv["status"] == "active":
                voss_reply(iid, iv["qid"], iv["part"])
            return self._json({"ok": True})

        if p == "/api/interview/reply":       # posted only by Claude, via curl
            db.interview_add_message(iid, "interviewer", b.get("text", ""))
            return self._json({"ok": True})

        if p == "/api/interview/code":
            db.interview_save_code(iid, b.get("code", ""))
            return self._json({"ok": True})

        if p == "/api/interview/end":
            iv = db.interview_get(iid)
            db.interview_end(iid)
            if iv:
                voss_review(iid, iv["qid"], iv["part"])
            return self._json({"ok": True})

        if p == "/api/interview/review":      # posted only by Claude, via curl
            db.interview_set_review(iid, b.get("review", ""))
            return self._json({"ok": True})

        self.send_error(404)

    # -- jobsearch/
    def _jobs_post(self, p, b):
        if p == "/api/jobs/manual":
            # No regex score computed on add -- queue it and run AI-judge
            # (on this posting or in a batch) to get a real fit score.
            company, title = b.get("company", "").strip(), b.get("title", "").strip()
            text, url = b.get("text", "").strip(), b.get("url", "").strip()
            location = b.get("location", "").strip()
            if not company or not title or not text:
                return self._json({"error": "company, title, and text are required"}, 400)
            jid, is_new = jobsdb.jobs_add("manual", company, title, url, text, location)
            return self._json({"job_id": jid, "is_new": is_new})

        if p == "/api/jobs/queue":
            jobsdb.jobs_set_status(int(b.get("job_id", 0)), "queued")
            return self._json({"ok": True})

        if p == "/api/jobs/mark-applied":
            # Direct "I already applied" for the common case that never
            # touches the generate pipeline at all -- applied on the
            # company's site or LinkedIn with your own resume. Without this,
            # only a job that went through Queue -> generate -> mark-applied
            # could ever be excluded from the Postings list, so a real
            # applied-to job kept resurfacing.
            jid = int(b.get("job_id", 0))
            if not jobsdb.jobs_get(jid):
                return self._json({"error": "no such job"}, 404)
            aid = jobsdb.applications_quick_mark_applied(
                jid, b.get("via", "other"), b.get("referral_name", ""), b.get("referral_note", ""))
            return self._json({"ok": True, "application_id": aid})

        if p == "/api/jobs/refetch":
            # Fetch only -- no scoring of any kind happens here anymore.
            # Regex's only job is the title/seniority/location/date
            # classification fetch_ats.py already applies before saving a
            # row at all. The only real fit judgment is the LLM one, a
            # separate, explicit action (AI-judge / /api/jobs/llm-backfill)
            # on purpose -- it has a real cost per call.
            # Async -- see _run_fetch_async. Starts the background fetch and
            # returns immediately; the UI polls /api/jobs/refetch/status.
            started = _run_fetch_async()
            if not started:
                return self._json({"error": "A fetch is already running"}, 409)
            return self._json({"started": True})

        if p == "/api/jobs/llm-backfill":
            # The ONLY place an LLM call happens -- fully manual, triggered
            # by this button, never automatically on fetch. Judges any real
            # engineering title (not manager-tier) in India/Bangalore/
            # Bengaluru that's either never been judged, OR was judged
            # before your point bank/skills/profile last changed
            # (jobs_llm_score_stale) -- same eng+seniority classification
            # the Postings list itself filters on, not a separate narrower
            # title-pattern gate (that gate matched only 4 literal title
            # shapes and, checked live, 0 of 302 real India-engineering
            # postings -- too narrow to be the real target population) --
            # editing the master doc doesn't retroactively fix old scores
            # on its own, this is the explicit "go re-check them" action.
            # Runs the whole eligible set, async -- no per-call cap, see
            # _run_judge_batch. date: optional, same today/week/2weeks
            # values as the list filter -- only set by the "Clear & re-judge"
            # flow, so clearing old scores doesn't also mean re-spending on
            # postings from months back that are probably long closed.
            profile, skill_names, _bullets_text, _skills_line, candidate_years = _jobs_resume_body()
            # Company/role/dates included, not just bullet text -- the judge
            # prompt is asked to weigh recency (a skill from a recent,
            # sustained role outweighs the same skill from a short stint
            # years ago), which is impossible to judge from bare text with
            # no dates attached.
            points_text = "\n".join(
                f"[{p['company']} -- {p['role']}, {p['start_date']} to {p['end_date'] or 'present'}] {p['text']}"
                for p in jobsdb.point_bank_all() if not p["text"].lstrip().startswith("["))
            date_filter = b.get("date", "")
            date_cutoff = {"today": _day_start_ts(0), "week": _day_start_ts(7),
                            "2weeks": _day_start_ts(14)}.get(date_filter)
            candidates = [j for j in jobsdb.jobs_list()
                          if jobs_ats.is_likely_engineering_title(j["title"])
                          and jobs_ats.title_seniority_tier(j["title"]) != "manager"
                          and jobsdb.jobs_llm_score_stale(j)
                          and jobs_ats.is_target_location(j["location"], j["description_raw"])
                          and (date_cutoff is None or j["fetched_ts"] >= date_cutoff)]
            started = _run_judge_batch(candidates, profile, points_text, skill_names, candidate_years)
            if not started:
                return self._json({"error": "AI-judge is already running"}, 409)
            return self._json({"started": True, "candidates": len(candidates)})

        if p == "/api/jobs/generate-queued":
            # The real fix for "I queued it and nothing happened" -- this is
            # what the Queue button always should have kicked off. Drafts +
            # renders every job currently status='queued', async (see
            # _run_generate_batch); each one costs a real Zai call, same
            # cost-per-action honesty as AI-judge.
            profile, skill_names, _bullets_text, _skills_line, candidate_years = _jobs_resume_body()
            points_text = "\n".join(
                f"[{p['company']} -- {p['role']}, {p['start_date']} to {p['end_date'] or 'present'}] {p['text']}"
                for p in jobsdb.point_bank_all() if not p["text"].lstrip().startswith("["))
            candidates = [j for j in jobsdb.jobs_list() if j["status"] == "queued"]
            started = _run_generate_batch(candidates, profile, points_text, skill_names, candidate_years)
            if not started:
                return self._json({"error": "A generate batch is already running"}, 409)
            return self._json({"started": True, "candidates": len(candidates)})

        if p == "/api/jobs/clear-llm-scores":
            # Explicit "force reload" -- wipes AI scores so the next
            # AI-judge run re-checks them from scratch, for when you'd
            # rather do a clean sweep than rely on the staleness check.
            # date: optional max-age scope (today/week/2weeks) -- clearing
            # everything back to day one meant the next judge pass would
            # burn real LLM calls re-checking postings from 10 weeks back
            # that are probably closed by now.
            date_filter = b.get("date", "")
            date_cutoff = {"today": _day_start_ts(0), "week": _day_start_ts(7),
                            "2weeks": _day_start_ts(14)}.get(date_filter)
            n = jobsdb.jobs_clear_llm_scores(date_cutoff=date_cutoff)
            return self._json({"cleared": n})

        if p == "/api/jobs/generate":          # posted only by Claude, via curl -- the
            # resume/cover letter text is authored per job, never auto-templated
            with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
                json.dump({"job_id": b.get("job_id"), "summary": b.get("summary", ""),
                           "cover_letter_body": b.get("cover_letter_body", "")}, f)
                req_path = f.name
            try:
                out = subprocess.run(
                    [JOBSEARCH_VENV_PY, os.path.join(JOBSEARCH_DIR, "generate_cli.py"), req_path],
                    capture_output=True, text=True, timeout=30, cwd=JOBSEARCH_DIR,
                )
            finally:
                os.unlink(req_path)
            if out.returncode != 0:
                return self._json({"error": (out.stderr or out.stdout)[-2000:]}, 500)
            try:
                return self._json(json.loads(out.stdout.strip().splitlines()[-1]))
            except Exception:
                return self._json({"error": f"bad generate_cli output: {out.stdout} {out.stderr}"}, 500)

        if p == "/api/applications/mark-applied":
            jobsdb.applications_mark_applied(int(b.get("app_id", 0)), b.get("via", "other"),
                                              referral_name=b.get("referral_name", ""),
                                              referral_note=b.get("referral_note", ""))
            return self._json({"ok": True})

        if p == "/api/applications/add-status":
            jobsdb.applications_add_status_event(int(b.get("app_id", 0)), b.get("status", ""),
                                                  note=b.get("note", ""))
            return self._json({"ok": True})

        if p == "/api/profile/points/rewrite-check":
            # Check-only -- never writes to the DB. Runs BEFORE a save so the
            # UI can show the full proposed rewrite (or exactly what's
            # missing) and let him decide, rather than silently flagging a
            # bullet after the fact. Never invents a fact, per his own spec.
            text = (b.get("text") or "").strip()
            if not text:
                return self._json({"status": "error", "message": "no text given"})
            result = _zai_rewrite_point(text, b.get("company", ""), b.get("role", ""))
            if result is None:
                return self._json({"status": "error",
                                    "message": "Zai call failed -- couldn't check this bullet right now. "
                                                "You can retry, or save as written."})
            return self._json(result)

        if p == "/api/profile/points/add":
            tags = [t.strip() for t in b.get("tags", "").split(",") if t.strip()]
            # not a hard block -- a manual, deliberate single add should go
            # through even if it looks similar, but the UI surfaces the
            # warning so a real accidental repeat doesn't sit unnoticed.
            match, ratio = jobsdb.point_bank_find_similar(b.get("text", ""))
            # Keyword+Use+Result rubric, authoring-time half: whether a
            # bullet has a brag/result and isn't a bare generic-verb opener
            # is true or false independent of any job -- catch it HERE,
            # when it's written, rather than only ever discovering it later
            # when generate() silently cuts it from some future resume.
            quality = jobs_ats.check_bullet_brag(b.get("text", ""))
            pid = jobsdb.point_bank_add(b.get("company", ""), b.get("role", ""), b.get("start_date", ""),
                                         b.get("end_date") or None, b.get("text", ""), tags=tags,
                                         source_note=b.get("source_note", "added via Profile UI"))
            return self._json({"id": pid, "possible_duplicate_of": match["text"] if match else None,
                                "quality_warning": None if quality["ok"] else quality})

        if p == "/api/profile/points/update":
            quality = jobs_ats.check_bullet_brag(b.get("text", ""))
            jobsdb.point_bank_update_text(int(b.get("id", 0)), b.get("text", ""))
            return self._json({"ok": True, "quality_warning": None if quality["ok"] else quality})

        if p == "/api/profile/points/toggle":
            jobsdb.point_bank_set_active(int(b.get("id", 0)), bool(b.get("active", True)))
            return self._json({"ok": True})

        if p == "/api/profile/skills/add":
            jobsdb.skills_add(b.get("name", ""), b.get("category", "Other"))
            return self._json({"ok": True})

        if p == "/api/profile/fields/set":
            jobsdb.profile_set(b.get("key", ""), b.get("value", ""))
            return self._json({"ok": True})

        if p == "/api/profile/extract":
            return self._profile_extract(b)

        if p == "/api/profile/extract/commit":
            return self._profile_extract_commit(b)

        self.send_error(404)

    def _profile_extract(self, b):
        """Upload OR pasted text -> extract -> propose. Never writes to the
        DB -- returns the proposal for the UI to show as an editable review
        list, each point marked with whether it looks like a near-duplicate
        of something already on file."""
        pasted_text = (b.get("text") or "").strip()
        if pasted_text:
            text = pasted_text
        else:
            filename = b.get("filename", "upload")
            content_b64 = b.get("content_base64", "")
            ext = os.path.splitext(filename)[1].lower()
            if ext not in (".pdf", ".docx", ".txt", ".md"):
                return self._json({"error": f"unsupported file type {ext or '(none)'} -- use .pdf, .docx, .txt or .md"}, 400)
            try:
                raw = base64.b64decode(content_b64)
            except Exception:
                return self._json({"error": "bad file upload"}, 400)

            with tempfile.NamedTemporaryFile(suffix=ext, delete=False) as f:
                f.write(raw)
                tmp_path = f.name
            try:
                text = _extract_file_text(tmp_path, ext)
            finally:
                os.unlink(tmp_path)

        if not text.strip():
            return self._json({"error": "no text found -- paste some text or choose a different file"}, 400)

        existing = jobsdb.point_bank_all(active_only=False)
        known_companies = sorted({p["company"] for p in existing})
        existing_points_text = "\n".join(f"- [{p['company']}] {p['text']}" for p in existing)
        proposal, err = _zai_extract_profile(text, known_companies, existing_points_text)
        if err:
            return self._json({"error": err}, 500)

        # Deterministic safety net on top of the LLM's own dedup instinct --
        # difflib against what's actually on file, not just what the model
        # noticed. Marks, doesn't drop: the user still sees and decides.
        for pt in proposal.get("points", []):
            match, ratio = jobsdb.point_bank_find_similar(pt.get("text", ""))
            pt["likely_duplicate"] = match is not None
            pt["duplicate_of_text"] = match["text"] if match else None
        return self._json({"proposal": proposal})

    def _profile_extract_commit(self, b):
        """Writes exactly the points/skills the UI sends back -- after the
        user has reviewed, edited, and deselected whatever they didn't want.
        Still re-checks each point against the point bank right before
        insert (skip if a near-duplicate now exists -- catches the case
        where two proposals from different uploads both slipped a near-
        identical point through review and got committed back to back)."""
        points = b.get("points", [])
        skills = b.get("skills", [])
        added_points, skipped_dupes, added_skills = 0, 0, 0
        for p in points:
            match, _ = jobsdb.point_bank_find_similar(p.get("text", ""))
            if match:
                skipped_dupes += 1
                continue
            tags = p.get("tags") or []
            if isinstance(tags, str):
                tags = [t.strip() for t in tags.split(",") if t.strip()]
            jobsdb.point_bank_add(p.get("company", ""), p.get("role", ""), p.get("start_date", ""),
                                   p.get("end_date") or None, p.get("text", ""), tags=tags,
                                   source_note="uploaded + extracted via Profile UI")
            added_points += 1
        for s in skills:
            jobsdb.skills_add(s.get("name", ""), s.get("category", "Other"))
            added_skills += 1
        return self._json({"added_points": added_points, "skipped_dupes": skipped_dupes,
                            "added_skills": added_skills})

    def do_POST(self):
        u = urlparse(self.path)
        p = u.path
        b = self._body()

        if p.startswith("/api/interview/"):
            return self._interview_post(p, b)

        if p.startswith("/api/jobs/") or p.startswith("/api/applications/") or p.startswith("/api/profile/"):
            return self._jobs_post(p, b)

        if p == "/api/ask":
            aqid, apart = b.get("qid", ""), int(b.get("part", 0))
            aq = find_question(aqid)
            if not aq:
                return self._json({"error": "no such question"}, 404)
            history = b.get("history", [])
            question = b.get("question", "")
            system_prompt = teacher_persona(load_pack(aq["topic"])) + "\n\n" + _question_context(aqid, apart)
            answer = _llm_call(system_prompt, history + [{"role": "user", "content": question}], max_tokens=900)
            if answer is None:
                return self._json({"error": "No LLM provider is configured yet."})
            return self._json({"answer": answer})

        qid, part = b.get("id", ""), int(b.get("part", 0))
        q = self._find(qid)

        if p == "/api/code":
            db.save_code(qid, part, b.get("code", ""))
            return self._json({"ok": True})

        if p == "/api/star":
            if b.get("starred"):
                db.star_set(qid, b.get("note", ""))
            else:
                db.star_remove(qid)
            return self._json({"ok": True})

        if p == "/api/reveal":
            kind = "reveal_solution" if b.get("what") == "solution" else "reveal_hint"
            db.log(qid, part, kind, str(b.get("index", "")))
            payload = {"ok": True}
            if kind == "reveal_solution" and q:
                if q.get("kind") == "judged":
                    solution_name = "rubric.md"
                elif q.get("lang") == "java":
                    solution_name = "solution.java"
                else:
                    solution_name = "solution.cpp"
                payload["solution"] = _read(
                    os.path.join(q["dir"], "parts", str(part), solution_name))
            return self._json(payload)

        if p in ("/api/run", "/api/submit"):
            if not q:
                return self._json({"error": "no such question"}, 404)
            pdir = os.path.join(q["dir"], "parts", str(part))
            code = b.get("code", "")
            db.save_code(qid, part, code)

            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "close")
            self.end_headers()

            def push(row):
                try:
                    self.wfile.write(("data: " + json.dumps(row) + "\n\n").encode())
                    self.wfile.flush()
                except OSError:
                    pass

            quick = p == "/api/run"
            try:
                if q.get("kind") == "judged":
                    res = judge_answer(q, part, code, push)
                elif q.get("lang") == "java":
                    res = java_runner.evaluate(code, pdir, stream=push, quick=quick)
                else:
                    res = runner.evaluate(code, pdir, stream=push, quick=quick)
            except Exception as ex:                       # never 500 mid-stream
                res = {"verdict": "REJECTED", "reason": f"harness error: {ex}", "stages": []}

            if not quick:
                kind = {"CLEAN": "submit_pass", "CORRECT_BUT_RACY": "submit_racy"}.get(
                    res["verdict"], "submit_fail")
                db.log(qid, part, kind, res.get("reason", ""))
                res["status"] = db.part_status(qid, part)
                res["label"] = db.label(res["status"])

            push({"stage": "__done__", "status": "done", "result": res})
            self.close_connection = True
            return

        self.send_error(404)


def main():
    os.makedirs(QUESTIONS, exist_ok=True)
    srv = ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    srv.daemon_threads = True
    n = len(load_questions())
    print(f"prep platform  ->  http://localhost:{PORT}   ({n} questions loaded)")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\nbye")


if __name__ == "__main__":
    main()
