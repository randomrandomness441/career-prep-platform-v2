# Architecture

This is the one document that explains how the whole system fits together.
`platform/AUTHORING.md` and `platform/PACK-FORMAT.md` cover authoring a
single question pack. `jobsearch/ats_scorer_spec.md` covers the ATS-scoring
design in isolation. This doc is the layer above those: how the two halves
are actually one running app, plus the handful of cross-cutting patterns
that do not belong in any single file's own comments.

Written so a cold session (or a future you) can get oriented in one read,
not so it has to re-derive any of this from scratch.

## 1. One process, two apps

`platform/server.py` is a single stdlib HTTP server, no framework, that
serves both halves:

- `/prepare/` routes to the C++ concurrency / interview-prep platform
  (`platform/static/`, `platform/questions/`).
- `/jobs/` routes to the job-application pipeline, which actually lives in
  a separate `jobsearch/` project and gets served from there
  (`server.py:1549-1553`).

`jobsearch/` is pulled in by path, not by package install. `JOBSEARCH_DIR`
has to resolve correctly under two different real layouts. On this
machine, `jobsearch/` sits next to `cpp/`, a holdover from before the two
projects were merged into one served app. A fresh clone of the public repo
is flat instead: `platform/` and `jobsearch/` as direct siblings.

`server.py:57-58` tries the flat layout first (`_flat_jobsearch`, a
sibling of `platform/` itself) and only falls back to the
sibling-of-`cpp/` layout if that one does not exist. Verified both ways:
this machine's `JOBSEARCH_DIR` is identical to before the fix, and a real
clone of the public repo into a throwaway directory starts and serves both
`/prepare/` and `/jobs/` with zero configuration.

## 2. The LLM provider fallback chain

Every LLM-backed job in the jobsearch half (judge a posting, draft a
resume and cover letter, rewrite a point-bank bullet, extract points from
an uploaded resume) routes through one function: `_llm_json_call`
(`server.py:624`). Call sites never talk to a provider directly.

**The chain, in order:**

1. `_zai_call` (`server.py:540`). Z.ai, primary. Cheap and fast, tuned for
   this app's actual volume: judging dozens to hundreds of postings in a
   batch.
2. `_anthropic_call` (`server.py:437`). A real Anthropic API key, if
   `ANTHROPIC_API_KEY` is ever set. Not configured by default.
3. `_claude_cli_call` (`server.py:507`). Shells out to the `claude` CLI
   itself, authenticated with a long-lived OAuth token
   (`CLAUDE_CODE_OAUTH_TOKEN`, from `claude setup-token`) instead of a
   billed API key. This is the true no-added-cost fallback: it draws on an
   existing Claude subscription's own usage window rather than a
   dollar-metered account. It is the same mechanism Anthropic's own
   `claude-code-action` GitHub Action uses for CI.

If Z.ai fails, `_llm_json_call` does not retry it on every subsequent
call. `_zai_cooldown_until` and `_ZAI_COOLDOWN_SECONDS`
(`server.py:620-621`, 300 seconds) skip straight to the Claude tiers for a
while, then try Z.ai again on its own. This keeps a batch from re-paying
Z.ai's own retry and backoff cost on every single call once it is known to
be down.

**Two model tiers per provider, not one model everywhere:**

- Z.ai's cheap default is `ZAI_MODEL`, which is `glm-5.3-flash`
  (`server.py:191`). Its full tier is `ZAI_MODEL_FULL`, which is
  `glm-5.3` (`server.py:199`).
- Claude's cheap default is `ANTHROPIC_MODEL_FALLBACK`, which is Haiku 4.5
  (`server.py:185`). Its full tier is `ANTHROPIC_MODEL_FULL`, which is
  Sonnet 5 (`server.py:188`).

`_zai_judge_job` (`server.py:701`), `_zai_draft_application` (`:791`), and
`_zai_extract_profile` (`:1090`) all use the cheap default. This is real,
high-volume classification and drafting work, not the place to spend on
the strongest model. `_zai_rewrite_point` (`:861`), the one call site that
writes into the master point-bank document itself, explicitly requests the
full tier on every provider. A bad answer there corrupts the one source of
truth everything else gets generated from. A bad answer from the judge or
draft calls is just one resume, regenerable on request.

(`ANTHROPIC_MODEL` at `server.py:177`, unqualified, is a different thing
entirely: the mock-interview "Voss" persona's own model, untouched by any
of this. It is a live conversational feature, not a batch JSON task, so it
does not belong in the cheap tier.)

## 3. Data and code are physically separate

One real incident drove this. A one-time personal seed script
(`jobsearch/seed_point_bank.py`, with a real name, phone number, and email
hardcoded in it) was committed to the very first commit of the public
snapshot repo, before this pattern existed.

The fix is not "remember to exclude it." That is exactly what failed
twice. It is structural instead: every database, every generated resume
and cover letter, and that one personal script now live under a single
`data/` folder per project (`platform/data/`, `jobsearch/data/`) and
nowhere else. `db.py` in each project creates its own `data/` on import:
`DATA_DIR = os.path.join(..., "data")`, then `os.makedirs(DATA_DIR,
exist_ok=True)`.

The public snapshot repo, this one, is rebuilt from the two live source
directories by `sync.sh` at the repo root. It excludes exactly one thing,
`data/`, instead of a hand-maintained list of `*.db` and `applications/`
patterns. `sync.sh` is the single source of truth for what gets published.
It is version-controlled right here, not retyped from memory in a
terminal each time.

## 4. The jobsearch pipeline, end to end

**A job's lifecycle:** fetched (Greenhouse, Lever, and Ashby APIs, see
`companies.py` for the tracked-company list), judged by the LLM
(`_zai_judge_job`, which scores fit against the real point bank and
explains the gap), shortlisted by hand, drafted on request, then tracked
through a real status funnel (applied, phone screen, interview, offer, and
so on).

**Generating one resume** (`generate.py:200`, `generate()`):

1. The humanizer gate runs on the drafted summary and cover letter. It is
   a hard stop, not a warning, before anything renders.
2. `select_bullets_max_ats()` (`generate.py:73`) greedily picks
   point-bank bullets by marginal ATS-score contribution for this
   specific posting.
3. `_cluster_duplicates()` (`generate.py:40`, `DUPLICATE_THRESHOLD =
   0.82`, difflib `SequenceMatcher` on normalized text) groups
   near-duplicate bullets first, so the greedy picker can only ever take
   one bullet per cluster. A near-identical rephrasing of an
   already-picked accomplishment cannot also land on the same resume.
4. A hard keyword and brag filter cuts any bullet with no JD keyword and
   no quantified result: the Keyword+Use+Result rubric enforced in code,
   not just in the drafting prompt (`ats.check_bullet_brag`, `ats.py:521`).
5. `_group_into_roles()` (`:130`) reshapes the selection back into
   per-company, per-role groups for rendering.
6. A real one-page trim loop runs next (`_render_and_count`,
   `_lowest_value_row_id`, `_drop_row`, `:168-196`, `MAX_PAGES = 1` at
   `:62`): render, check the real PDF page count, drop the single
   lowest-marginal-value bullet if it overflowed, repeat. It is bounded by
   the number of bullets, so it always terminates.
7. The final resume is re-scored against `ats.score_resume()` for the
   number shown in the UI, and the pipeline records exactly which real
   accomplishments got cut and why (`result["weak_bullets"]`). Nothing is
   silently dropped.

The full keyword-coverage scoring formula (evidence weighting, the
quantified-bullet bonus, the too-few-matches penalty) lives in
`ats_scorer_spec.md` and in `ats.py`'s own comments, not repeated here.
One thing is worth stating plainly, since it is easy to misread as a bug:
the LLM judge's score and a generated application's `ats_estimate.score`
are two different, deliberately separate numbers. The judge reasons
semantically, so transferable skills count. The ATS estimate counts
literal keyword text matches only. A posting can score high on one and
low on the other. That gap is the documented reason the LLM judge is the
primary signal at all, not a sign that either number is wrong.

## 5. One style bar, enforced everywhere

`humanizer.check_human_style()` (`humanizer.py:36`) is a hard, code-level
gate: no em dashes, no banned AI-slop words or phrases, and none of the
hedge-then-reveal sentence construction that `HUMAN_STYLE_RULES` bans by
name. It runs against every LLM-drafted resume summary and cover letter.
The same function, the same rule set, also ran against every piece of
hand-written public copy that went into this repo this session: the
README, the social-preview tagline, the repo's GitHub description, and
this document itself. One quality bar, regardless of who or what wrote
the prose, and regardless of whether the text is user-facing product copy
or internal engineering documentation.

## 6. The Curie identity

The public repo is named Curie, after Marie Curie, chosen for being a
genuinely multidisciplinary figure to match a platform that teaches real
concepts, judges job fit, and writes resumes, all three, not a narrow
single-purpose tool. The logo is a periodic-table tile for Curium (`Cm`,
atomic number 96), the actual element named for the Curies, not a generic
atom icon.

One important operational fact: this repo is a snapshot, not a live
mirror. It gets rebuilt from `cpp/platform/` and `jobsearch/` by
`sync.sh` on demand. Editing files directly in this repo and expecting
them to flow back to the live source would be backwards. The live source
directories are the source of truth. This repo is their published output.
