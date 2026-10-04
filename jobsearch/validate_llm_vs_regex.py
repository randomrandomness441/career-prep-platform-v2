#!/usr/bin/env python3
"""Three-way validation: regex alone (already computed) vs GLM-4.6 given the
regex score as context ("regex+llm") vs GLM-4.6 with NO regex score in the
prompt at all ("llm alone"). Two separate LLM variants on purpose -- telling
the model "a keyword pass already scored this 75" risks anchoring its
answer toward that number, which would make "regex+llm" a weaker, biased
check rather than an independent second opinion. Comparing both against
plain regex is what actually answers "is the regex pre-filter reliable."

Must run in the terminal where ZAI_API_KEY is exported -- the key never
passes through Claude or this repo.

    cd jobsearch
    .venv/bin/python3 validate_llm_vs_regex.py              # all fetched postings
    .venv/bin/python3 validate_llm_vs_regex.py --limit 100  # a subset, faster

Writes each result as one line to validate_results.jsonl AS IT GOES (not
just at the end), so Ctrl-C or a crash partway through loses nothing already
done -- re-running skips ids whose BOTH calls already succeeded, retries ids
where either one errored. Prints progress with an ETA.
"""
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request

import ats
import db

ZAI_API_KEY = os.environ.get("ZAI_API_KEY", "")
ZAI_MODEL = os.environ.get("ZAI_MODEL", "glm-4.6")
# /coding/paas/v4/, not /api/paas/v4/ -- that's the endpoint the GLM Coding
# Lite subscription actually covers. The general one bills from a separate
# pay-as-you-go wallet and 429s "Insufficient balance" even with a valid,
# funded coding-plan key. Confirmed by hitting that exact error.
ZAI_URL = "https://api.z.ai/api/coding/paas/v4/chat/completions"
OUT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "validate_results.jsonl")

JD_CHAR_LIMIT = 12000   # covers all but the single longest of 963 postings in full


def trim_jd(text, limit=JD_CHAR_LIMIT):
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


SYSTEM = ('You score how well a real candidate fits one job posting. Be honest and '
          'specific -- name the real gap if there is one, never inflate. Reply with '
          'ONLY a JSON object: {"score": <0-100 int>, "reason": "<one sentence>"}.')


def _candidate_block(profile, points_text, skill_names, candidate_years):
    return (f"Candidate: {profile.get('title', '')}, {candidate_years} years of real experience.\n\n"
            f"Real accomplishments:\n{points_text}\n\n"
            f"Known skills: {', '.join(skill_names)}\n\n")


def _call_zai(user_content):
    """Shared HTTP call -- both prompt variants use this. thinking disabled:
    reasoning tokens draw from the same max_tokens budget, and a short
    scoring judgment doesn't need extended thinking; a small budget with
    thinking on can come back with zero visible output."""
    body = json.dumps({
        "model": ZAI_MODEL, "max_tokens": 500, "temperature": 0.2,
        "thinking": {"type": "disabled"},
        "messages": [{"role": "system", "content": SYSTEM}, {"role": "user", "content": user_content}],
    }).encode()
    req = urllib.request.Request(
        ZAI_URL, data=body, method="POST",
        headers={"Authorization": f"Bearer {ZAI_API_KEY}", "content-type": "application/json"})

    delay = 1.0
    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                data = json.loads(resp.read())
            reply = data["choices"][0]["message"]["content"].strip()
            m = re.search(r"\{.*\}", reply, re.S)
            obj = json.loads(m.group(0) if m else reply)
            return int(obj["score"]), str(obj.get("reason", ""))[:600], None
        except urllib.error.HTTPError as e:
            body_text = e.read().decode(errors="replace")
            if e.code == 429 and attempt < 2:
                time.sleep(delay)
                delay *= 2
                continue
            return None, None, f"HTTP {e.code}: {body_text[:200]}"
        except Exception as e:
            return None, None, str(e)
    return None, None, "exhausted retries"


def judge_with_regex(job, profile, points_text, skill_names, candidate_years, regex_score):
    user = (
        _candidate_block(profile, points_text, skill_names, candidate_years) +
        f"A free keyword-matching pass already scored this posting {regex_score}/100 on hard-skill "
        f"keyword overlap alone. Your job is different: read the actual posting and judge real fit -- "
        f"synonyms the keyword pass would miss, seniority/scope match, anything the raw keyword "
        f"count gets wrong in either direction.\n\n"
        f"Job posting -- {job['title']} at {job['company']}:\n{trim_jd(job['description_raw'])}"
    )
    return _call_zai(user)


def judge_alone(job, profile, points_text, skill_names, candidate_years):
    """No regex score anywhere in this prompt -- the independent check.
    Comparing this against judge_with_regex's output on the same posting is
    what tells us whether showing the regex number anchors the LLM's own
    answer toward it."""
    user = (
        _candidate_block(profile, points_text, skill_names, candidate_years) +
        f"Read the actual posting and judge real fit for this candidate -- synonyms a naive keyword "
        f"search would miss, seniority/scope match, anything a shallow pass would get wrong.\n\n"
        f"Job posting -- {job['title']} at {job['company']}:\n{trim_jd(job['description_raw'])}"
    )
    return _call_zai(user)


def main():
    if not ZAI_API_KEY:
        print("ZAI_API_KEY not set in this shell -- export it first, then re-run.")
        sys.exit(1)

    profile = db.profile_all()
    skill_names = [s["name"] for s in db.skills_all()]
    points_text = "\n".join(p["text"] for p in db.point_bank_all() if not p["text"].lstrip().startswith("["))
    candidate_years = float(profile.get("years_experience", 0) or 0)

    jobs = db.jobs_list()
    # Default scope: the tight title pattern alone (SDE-2/II, Software
    # Engineer 2/II, or Senior Software Engineer -- not "senior anything
    # engineer"). No years-based exclusion here either -- the 40-posting
    # validation run showed the regex years extraction is exactly as
    # unreliable as the score itself (both get read directly by the LLM
    # from the JD text instead, which weighs a "4.5 vs 5+" gap
    # proportionately rather than as a hard cutoff). Pass --all to
    # validate the full fetched set instead.
    if "--all" not in sys.argv:
        jobs = [j for j in jobs if ats.matches_target_title(j["title"])]
    if "--limit" in sys.argv:
        jobs = jobs[:int(sys.argv[sys.argv.index("--limit") + 1])]

    already_done = set()
    prior_errors = 0
    if os.path.exists(OUT_PATH):
        with open(OUT_PATH) as f:
            for line in f:
                try:
                    row = json.loads(line)
                    # only "done" if BOTH variants succeeded -- a partial
                    # failure (one call ok, the other errored) gets retried
                    # whole rather than left with a missing half.
                    if row.get("error_with_regex") or row.get("error_alone"):
                        prior_errors += 1
                        continue
                    already_done.add(row["id"])
                except Exception:
                    pass
        print(f"resuming: {len(already_done)} already done, {prior_errors} prior errors will be retried")

    todo = [j for j in jobs if j["id"] not in already_done]
    print(f"model={ZAI_MODEL}  total={len(jobs)}  todo={len(todo)}  (2 calls per posting)")

    t0 = time.time()
    errors = 0
    with open(OUT_PATH, "a") as out:
        for i, j in enumerate(todo):
            wr_score, wr_reason, wr_err = judge_with_regex(
                j, profile, points_text, skill_names, candidate_years, j["match_score"])
            alone_score, alone_reason, alone_err = judge_alone(
                j, profile, points_text, skill_names, candidate_years)

            row = {
                "id": j["id"], "company": j["company"], "title": j["title"],
                "regex_score": j["match_score"],
                "llm_with_regex_score": wr_score, "llm_with_regex_reason": wr_reason,
                "error_with_regex": wr_err,
                "llm_alone_score": alone_score, "llm_alone_reason": alone_reason,
                "error_alone": alone_err,
            }
            out.write(json.dumps(row) + "\n")
            out.flush()
            if wr_err or alone_err:
                errors += 1

            elapsed = time.time() - t0
            rate = (i + 1) / elapsed if elapsed > 0 else 0
            eta_min = (len(todo) - i - 1) / rate / 60 if rate > 0 else 0
            status = (f"with_regex={wr_score} alone={alone_score}" if not (wr_err or alone_err)
                      else f"ERROR: {(wr_err or alone_err)[:50]}")
            print(f"[{i+1}/{len(todo)}] {j['company']:16} {j['title'][:35]:35} "
                  f"regex={j['match_score']:>5} {status}   (~{eta_min:.0f} min left)")

    print(f"\ndone. {len(todo)} postings judged this run, {errors} had an error. Results in {OUT_PATH}")


if __name__ == "__main__":
    main()
