# Building your own skill pack

This platform was originally built for one person's C++ concurrency prep. The engine
(`server.py`, `db.py`, `runner.py`, everything under `static/`) doesn't know anything
about concurrency specifically — it just serves whatever's inside `questions/<topic>/`.
That means you can drop your own topic in next to (or instead of) the existing ones —
Spark tuning, HLD/LLD, Iceberg internals, anything — and get the same run/submit flow,
progress tracking, star+note, mock-interview-style discussion, and "ask a teacher" panel
for free. No engine code to touch.

`questions/design/001-rate-limiter/` in this repo is a small working example of
everything below. Read it alongside this doc, or copy it as a starting point.

## The two question kinds

Every question is one of two kinds, set in `meta.json`:

- **`"kind": "compiled"`** (the default — omit the field entirely and you get this).
  The candidate writes real code, a compiler and a test binary decide pass/fail. This
  is what the original C++ concurrency questions use. Right now the engine only knows
  how to compile C++ (`runner.py` shells out to `clang++`) — a pack in another compiled
  language needs real changes to `runner.py`, which is out of scope for this doc.
- **`"kind": "judged"`.** The candidate writes a free-text answer — a design writeup,
  a written explanation, whatever the question calls for — and an LLM grades it against
  a short rubric you write. No compiler involved at all. This is the kind almost
  everything outside "write compileable code" should use: system design, conceptual
  questions, tuning discussions, anything judged by "is this reasoning good," not
  "does this compile and pass."

## Folder layout

A pack is one folder under `questions/`:

```
questions/<your-topic>/
  pack.json                      -- optional, see below
  <slug>/
    meta.json
    reading.md                   -- optional background reading, shown on its own tab
    parts/0/
      statement.md                the question itself
      usage.md                    optional, a worked example of the calling convention
      hints.md                    optional, hints separated by lines containing only ---

      -- for kind: "compiled" --
      boilerplate.cpp              a real skeleton, must fail the tests as given
      solution.cpp                 the correct version
      tests.cpp                    #include "solution.hpp" first, then real assertions

      -- for kind: "judged" --
      rubric.md                    what a good answer covers -- this is what the judge
                                    grades against, and what a "reveal solution" click
                                    shows the candidate instead of solution.cpp
```

One part per question is normal. Multiple parts (follow-ups on the same problem) work
the same way for either kind — just add `parts/1/`, `parts/2/`, etc.

### `meta.json`

```json
{
  "title": "Design a Rate Limiter",
  "difficulty": "Easy | Medium | Hard",
  "category": "design",
  "concepts": ["token bucket", "sliding window"],
  "est_minutes": 20,
  "kind": "judged"
}
```

`category` is a free-text label shown as a badge in the question list — pick whatever
word makes sense for your pack (`"design"`, `"tuning"`, `"internals"`, ...). `difficulty`
drives the Easy/Medium/Hard grouping in the list view. `companies` and `reported_as`
(both optional arrays) exist if a question is sourced from a real reported interview —
see any existing question for the pattern, skip them otherwise.

### `pack.json` (optional)

Sits at the top of your topic folder, next to the question subfolders. Controls the
name shown in the UI and the words used by the LLM-backed interviewer and teacher panel
when they're talking about your pack. Every field is optional — anything you leave out
falls back to a generic default, so an empty or missing `pack.json` still works.

```json
{
  "name": "Spark Performance Tuning",
  "topic_phrase": "Spark job tuning",
  "teacher_framing": "this Spark tuning course",
  "interviewer_name": "Voss",
  "interviewer_role": "senior data engineer"
}
```

- `name` — shown in the question list's per-pack progress chip and in the filter
  dropdown ("Pack: Spark Performance Tuning").
- `topic_phrase` — dropped into the interviewer's system prompt: "running a live
  technical interview on `{topic_phrase}`."
- `teacher_framing` — dropped into the "ask a teacher" panel's system prompt: "the
  teacher for `{teacher_framing}`."
- `interviewer_name` / `interviewer_role` — who the mock interview persona is. Keep
  "Voss" if you want the same persona across packs, or give your pack its own name.

### `rubric.md` (judged questions only)

Plain prose, no required structure — this is what gets shown to the candidate when
they reveal the "solution," and what the LLM judge is told to grade against. Be
specific about what a passing answer actually has to say, not just topics to mention —
see `questions/design/001-rate-limiter/parts/0/rubric.md` for the level of detail that
makes grading consistent.

## What you get automatically

- **Progress tracking** (`db.py`'s event log, `all_status`, star+note, interview
  history) works identically for every pack — it's derived from `qid`/`part`, which
  every question has regardless of kind or topic.
- **Per-pack progress rollup** in the question list — once more than one topic folder
  exists, a small chip per pack shows solved/total and doubles as a filter.
- **Mock interview and "ask a teacher"** both work for `judged` questions the same way
  they do for `compiled` ones, using your `pack.json` persona text — except mock
  interviews specifically are gated to `compiled` questions only (a live back-and-forth
  interview assumes code to react to; a `judged` question is graded once, on submit).

## What you don't get for free

- **A judge or teacher reply needs a configured LLM.** Set `LLM_PROVIDER` and the
  matching `*_API_KEY` environment variable before starting `server.py` (see the top
  of `server.py` for the current provider options). Without a key, `judged` submissions
  return a plain "no judge available" message instead of a verdict — the rest of the
  app still works, you just won't get graded answers.
- **A `compiled` pack in a language other than C++** needs real work in `runner.py`,
  which currently only knows how to invoke `clang++`. Nothing here makes that free.
