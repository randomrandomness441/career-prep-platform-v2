# Authoring brief — read this before writing any question

This file governs the existing `concurrency`/`general`/`qa` packs specifically. Adding a
whole new topic pack (a different skill entirely, not more concurrency content) follows a
different, much lighter format — see `PACK-FORMAT.md` instead.

You are authoring content for a local C++ concurrency interview-prep platform for one user,
Sourav, preparing for senior-level interviews. Source material for his original 33 questions
is `platform/source/concurrency-guide-33.md` (restored 2026-09-08). It is LLM-generated and
**contains verified bugs** (list below) — re-verify every solution taken from it by running it.

His goal, in his words: naive solution first, then the production-level one, with the story of
why the naive version *seems* correct but isn't. He is interviewing for a senior role; the
more production-flavoured the framing, the better.

## Rules that are not negotiable

1. **Absolute paths in every shell command.** A previous session lost work to `cd` drift.
2. **Never invent output or numbers.** Every piece of program output and every performance
   figure in a reading must come from code you actually compiled and ran.
3. **Verify before declaring done** (command below). `solution.cpp` must be CLEAN;
   `boilerplate.cpp` must be REJECTED (or CORRECT_BUT_RACY where noted).
4. **Do not touch question folders outside your assignment.** Other agents run in parallel.

## Format reference

Read these in full before starting:
- `/Users/sourav/Documents/cpp/platform/questions/concurrency/003-thread-ownership/` — a complete question
- `/Users/sourav/Documents/cpp/platform/questions/concurrency/005-scoped-lock/reading.md` — FULL-tier reading quality bar

## Folder layout

```
platform/questions/concurrency/<NNN-slug>/
  meta.json    {"title":..,"difficulty":"Easy|Medium|Hard","chapter":N,"concepts":[..],"est_minutes":N}
  reading.md
  parts/0/statement.md
  parts/0/usage.md          optional: a minimal main()-shaped snippet showing
                             how the interface gets called (which threads call
                             which methods) -- NOT the real test harness, just
                             enough that the calling convention isn't a guess
  parts/0/hints.md          3 hints, separated by lines containing only ---
  parts/0/boilerplate.cpp   the NAIVE version; must FAIL the tests
  parts/0/solution.cpp      the correct version
  parts/0/tests.cpp         includes "solution.hpp", has main(), 0 = pass
```

One part per question unless told otherwise. `usage.md` is now a standard part of every
question, old and new alike (he asked for the backfill explicitly on 2026-09-10 — a real
minimal C++ snippet, not literal pseudocode, showing which threads call which methods in
what shape). Add one whenever you touch a question for any reason in the retrofit passes
below, and for every new question going forward.

## Reading tiers

**FULL** — seven numbered sections, in order:
1. `## 1. Reframe the problem` — what the problem actually is, once stated correctly
2. `## 2. The tools` — each primitive in plain language with runnable snippets. Must contain
   enough for the reader to actually solve the problem.
3. `## 3. The broken version, first` — the plausible wrong solution, REAL output from running
   it, then why it fails. Must say explicitly **why it seems correct** — what makes the naive
   version seductive and who it fools — before showing the evidence that breaks it. Then the
   fix, and what the production version does differently and why that is the *expected*
   answer at senior level.
4. `## 4. Real-world usage` — where it is used in production, **why it was invented**, and
   **where NOT to use it**. All three, explicitly.
5. `## 5. Performance` — measured numbers, never asserted ones
6. `## 6. Where this solution fails` — edge cases, inputs and schedules that break it, when
   it is correct but too slow. Think hard here; this is the section the user values most.
7. `## 7. Interview follow-ups` — question/answer pairs, written as an interviewer would ask
   them. Seed with the guide's follow-ups where they exist, then add stronger ones he won't
   have seen. **Not "what does this line do" trivia** — scenario scaling:
   - small machine (1–2 cores, one core parked for the OS) — what changes?
   - big machine (64+ cores, NUMA, remote cache lines) — what breaks first?
   - extreme load (10^8 req/s, 100M elements) — which resource saturates first: cache line,
     allocator, syscall path, lock convoy, memory bandwidth?
   - failure injection (exception mid-operation, client disconnect, OOM, producer dies)
   - production ops (metrics to watch, graceful shutdown, backpressure, tuning knobs)
   Each follow-up needs a real answer, not a pointer.

**THIN** — sections 1, 3, 6 and 7 only. Keep the same headings and numbers.

## Verified bugs in the source guide — do not propagate

- **#9 Traffic Light (→022):** the guide's solution deadlocks — E/W traffic never crosses.
  Write a correct solution and a test that proves E/W actually crosses under load.
- **#16 Atomics vs Mutexes (→030):** claims an uncontended mutex costs "1000+ cycles".
  Measured here: ~26 cycles uncontended. The contended path is a different story — measure
  both, don't assert.
- **#22 jthread (→028):** the `cv.wait(lock, stop_token, pred)` overload exists only on
  `std::condition_variable_any`, not `std::condition_variable`.
- **#5 Bounded Queue (→023):** a signed/unsigned comparison fails `-Werror`.
- **#10 Classroom (→044):** the cited LeetCode ID is not that problem.

If a guide claim looks suspicious, measure it on this machine and use the measured number.

## tests.cpp rules

- `#include "solution.hpp"` first — the harness injects the candidate's file under that name
- then `#include "concur/stress.hpp"`, and call `SHAKE()` where a thread switch would be
  inconvenient. It compiles to nothing unless `-DSHAKE` is set.
- `main()` returns 0 on pass, non-zero on failure, after a clear `printf` diagnostic
- many trials; deterministic pass criteria
- **must finish in under 3 seconds** — the harness kills any run over 12s and calls it a hang
- compiled with `-Wall -Wextra -Wno-unused-parameter -Wshadow`; no warnings allowed
- **no benchmarks inside tests.cpp** — run those separately under /tmp and paste numbers into
  reading.md

### Making the naive version fail reliably

The test must fail the naive version essentially every run. Preferred, in order:
1. **A real correctness bug** the naive version has (best — deterministic).
2. **A missing-synchronisation data race**, caught by the harness's ThreadSanitizer stage.
3. **A deadlock**, caught by the watchdog.
4. Timing-dependent failure with enough trials — only if nothing above works, and only after
   you have measured that it fires on every run across at least 5 repeats.

**Known trap:** ThreadSanitizer does NOT reliably catch *memory-ordering* bugs — using
`memory_order_relaxed` where you needed acquire/release often produces no TSan warning at
all, because TSan models atomics as ordered. TSan catches *missing* synchronisation, not
*insufficient* ordering. Do not rely on it for memory-order questions.

## Verification (mandatory)

Run from `/Users/sourav/Documents/cpp`:

```
python3 -c "
import sys, os; sys.path.insert(0,'platform')
import runner
part='platform/questions/concurrency/<NNN-slug>/parts/0'
for n in ['solution.cpp','boilerplate.cpp']:
    r=runner.evaluate(open(os.path.join(part,n)).read(), part, quick=True)
    print('%-16s -> %s %s'%(n,r['verdict'],r.get('reason','')))
"
```

Drop `quick=True` to also run ThreadSanitizer and the shaken stress stage — required for any
lock-free or memory-ordering question, and for anything where the naive version's failure
mode is a race.

## Style

Plain language, for a strong C++ engineer who is new to concurrency. Never quote the book;
explain in your own words. Define every term on first use. Short sentences. No textbook
register. The user has said explicitly that quoting at him is not teaching.

## Code style — simple C++, concept-first (2026-09-10, he asked for this explicitly)

He knows concurrency concepts. He does **not** know advanced modern C++, and boilerplate
written in advanced C++ was blocking him from even reading the problem — he couldn't tell
where the concurrency ended and the C++ cleverness began. `boilerplate.cpp` is what he stares
at first; it is the highest-priority file to keep plain. `solution.cpp` matters too, since he
reveals it when stuck.

**Cut, unless the problem is literally about the thing being cut:**
- `template<typename T>` — hardcode a concrete type (usually `int`, or whatever the problem's
  own example uses) instead of templatizing. Exception: the problem *is* "build a generic
  container/pool/queue" — then keep a single, undecorated type parameter, nothing fancier
  (no traits, no `enable_if`, no variadic packs).
- `std::forward` / universal references (`T&&` + forwarding) — take by value or `const&`.
- `if constexpr`, SFINAE, CRTP, operator overloading beyond the obvious (`==`, stream `<<`).
- Clever one-liners (chained STL algorithms, fold-y lambdas) — an explicit loop a beginner can
  step through beats a one-liner that hides the loop.
- `template <typename F>` / `template <typename P>` for a callback parameter — that makes the
  reader reason about template instantiation for something that's just "a function you call".
  Use a concrete `std::function<Sig>` instead: it's a named type with a visible signature,
  which is the *less* abstract choice here even though it's technically another template
  underneath. (`std::function` itself only stays cut where nothing is actually being passed
  around — see above.)
- `std::jthread` + `stop_token` when the question isn't about cooperative cancellation —
  use plain `std::thread` with an explicit `.join()`.

**Keep, because it IS the concept being tested:**
- The concurrency primitives themselves — `mutex`, `atomic`, `condition_variable`,
  `semaphore`, `shared_mutex`, `latch`, `barrier`, `future`/`promise`/`async` — those are the
  syllabus, don't water them down. Simplify what's *around* them, not them.
- A function signature mandated by the problem's own stated interface (e.g. a LeetCode-style
  prompt that hands you `std::function<void()> printFirst` — that's the problem's contract,
  not incidental decoration; don't rewrite it away).
- Lock-free / CAS / memory-ordering machinery when the question is specifically about that
  (Treiber stack, SPSC ring buffer, hash map) — that complexity is the point of those
  questions. Still: comment every CAS loop and every `memory_order` choice in plain words.

**Always, on every mutex/atomic/condvar the boilerplate or solution introduces:** a one-line
comment naming the invariant it protects. "protects `queue_` so push and pop never interleave"
— not "for thread safety."

**If you change a class's public interface** (e.g. de-templatizing it), `tests.cpp` in that
same part must be updated to match and the part re-verified end to end — boilerplate REJECTED,
solution CLEAN, full (non-quick) run. Never leave `tests.cpp` calling an interface `solution.cpp`
no longer has.

This is a retrofit target for **all** existing questions, not just new ones — going through
them is tracked in `PROGRESS.md`.

## statement.md style — story first, spec second (2026-09-10, he asked for this explicitly)

His words: the questions read "in a very dense academic way" — a spec sheet, not something
written for a person. "What the question is testing is really good but to even understand
that, I have to spend a lot of time." He wants **fun, simple, a story, an example** — not
less technical content, just not delivered as a wall of numbered requirements with no
on-ramp. He later pasted a worked example of the shape he wants (see 009-timeouts's
statement.md, "the pizza delivery analogy" — use it as the template, not just this
description of it).

**Every `statement.md` gets these sections, in this order:**

1. **`## ELI5: the <X> analogy`** — 4-8 sentences, a concrete everyday scene a
   non-specialist could picture (pizza delivery, a kitchen pass-through shelf, a relay
   race, a shared notebook — whatever actually maps onto the problem's shape). Map the
   operations onto the story beat by beat (bullet points work well: "the pizza arrives —
   ...", "the chef yells from the back — ..."). Plain words only here — no "invariant",
   "atomicity", "contention", no type names. Name the one trap that makes the question
   interesting, inside the story, before any code appears.
2. **`## What you're actually building`** — the bridge from story to spec: the real
   interface code block (unchanged signatures), with each method's doc-comment written in
   a blend of the story's vocabulary and the real behavior, plus a called-out "the hard
   requirement" paragraph if there's one specific thing the question is testing.
3. **`## Requirements`** — still a numbered list, that structure is fine and searchable,
   but each one gets a plain-language clause and the story's vocabulary carries through
   where it fits ("everyone's pizza order gets fulfilled from the same delivery" rather
   than switching cold into "all waiters observe the value"). Fine for these to get more
   technical than the opening scene, since the reader now has the right picture in mind.
4. **`## Why the constraints exist`** (not just "Constraints") — every constraint gets a
   plain-English *why*, not just a *what*. "No sleep-and-poll loops — that burns CPU the
   whole time you're 'waiting,' like pacing in circles instead of sitting down" beats
   "No busy-waiting." Close with a one-paragraph "in short" that names the real C++
   mechanism being tested, so a reader who wants the technical summary alone has it.

**Keep every requirement.** This is a framing pass, not a content cut — nothing in
Requirements or Constraints gets dropped, softened into vagueness, or made less precise.
The measure of success: could a reader who is competent at C++ but has never met this
specific concurrency bug understand what's being asked, and why it's interesting, from the
ELI5 section alone — before they've parsed a single requirement?

Worked examples: `009-timeouts/parts/0/statement.md` (pizza delivery — his own draft) and
`023-bounded-blocking-queue/parts/0/statement.md` (kitchen pass-through shelf). This is a
retrofit target for all existing questions, same as the code-style pass above.

## Topics and category tags (2026-09-16, he asked for this explicitly)

He wants real questions from actual company interview experiences added, not just
concurrency, and a way to tell at a glance what kind of question each one is.

**`platform/questions/<topic>/`** — a topic is a folder, same mechanism as `concurrency`
already uses (`server.py`'s `load_questions()` reads every topic dir, no code change
needed for a new one). So far: `concurrency` (the primary one, NNN-slug numbering, full
AUTHORING rigor above), `general` (non-concurrency coding/DS/algo questions, descriptive
slugs, no number prefix), `qa` (conceptual/discussion questions, no meaningful "boilerplate
fails" story — see below).

**`meta.json["category"]`** — one of `"concurrency"` | `"coding"` | `"qa"`, on every
question, old and new. This is the axis he asked for ("is this a concurrency question or a
normal coding question or q/a") and is independent of which topic folder it lives in.

**`meta.json["companies"]`** (array) + **`meta.json["reported_as"]`** (array, exact
reported phrasing) whenever a question is a real, sourced interview question — see the
9 existing questions tagged `"companies": ["Pure Storage"]` for the pattern. Don't invent
a company tag for a question you made up; only for ones traced to an actual report.

**Effort scales to what the question actually is** — do not force every addition through
the full concurrency pipeline:
- `category: "concurrency"` — full rigor as specified everywhere else in this file:
  boilerplate REJECTED / solution CLEAN verified through TSan + stress, FULL or THIN
  reading tier as appropriate, ELI5 statement.md, usage.md.
- `category: "coding"` (general DS/algo, no threads) — same file layout and the same
  "naive version fails, fix it, verify" shape, but a **THIN** reading always (Reframe,
  broken version with real output, where it fails — skip tools/real-world/performance
  unless genuinely relevant), and there is nothing for TSan or the shaken-stress stage to
  usefully find in a single-threaded program — `runner.evaluate` still runs them
  harmlessly, don't hand-wave "verified" without actually running it regardless.
- `category: "qa"` — a conceptual question with a written answer, not a coding exercise.
  `statement.md` poses the question (ELI5 opener still applies — these benefit from it
  most). The real answer lives in `reading.md`, written as prose, no forced 7-section
  structure. Still needs a minimal, valid `boilerplate.cpp` / `solution.cpp` / `tests.cpp`
  so the existing run/submit machinery doesn't break — boilerplate can be a stub that
  intentionally fails (`return 1` with a comment pointing at Reading), solution a short
  illustrative snippet if one is natural to the question or just `return 0` if not, tests
  trivially checking the exit code. Don't pretend a one-line "what is a mutex" question is
  a rigorous coding exercise — a short, well-reasoned answer is the whole deliverable.
  Prefer folding a closely-related one-liner into an existing concurrency question's
  section 7 (interview follow-ups) over a whole new qa folder when the two would
  substantially overlap (see 001's and 030's reading.md for two such folds).
