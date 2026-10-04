## 1. Reframe the problem

Sequential code either produces the right answer or it doesn't, and running it twice tells
you nothing a single run didn't already. Concurrent code breaks that assumption completely,
correctness depends on *scheduling*, which your test suite doesn't control and usually can't
even observe, so "I ran it and it worked" is close to meaningless on its own. This question's
implementation (a three-line spinlock) is almost incidental; the actual subject is what it
takes to know, rather than hope, that concurrent code is correct, and this platform's own
runner, which every question in this course has been graded through, is a worked answer to
exactly that question.

## 3. The broken version, first

The boilerplate is the spinlock everyone writes first:

```cpp
while (locked_) {}
locked_ = true;
```

**Why it looks right:** it reads exactly like the English description, "wait until it's
free, then take it", and `locked_` genuinely does end up `true` after this code runs, which
is the only thing a glance at the code confirms. The bug is invisible to inspection because
it's not about what any single thread does; it's about what two threads can do *at the same
instant*, which source code doesn't show you.

Run against this question's own test, 8 threads, 2,000 increments each, directly counting
how many times more than one thread was inside the critical section at once:

```
mutual exclusion violated 13071 times: more than one thread was inside the
critical section at once
```

Not a rare, hard-to-catch accident, over 13,000 violations out of 16,000 attempts.
`while (locked_) {}` and `locked_ = true;` are two separate memory operations with an
unbounded gap between them from another thread's point of view; under real contention, both
threads passing the check before either writes is the *common* case, not the exception.

The fix collapses both steps into one atomic operation:

```cpp
while (locked_.exchange(true, std::memory_order_acquire)) { }
```

Same test: zero violations, every run.

## 6. Where this solution fails, and what it took to know that

This section is where this question earns its place: not more edge cases in the spinlock
itself, but an honest account of what actually caught bugs across this whole course, because
that's the real content.

- **A correctness test that runs the code once and checks the final answer is the weakest
 signal, and it's often not weak enough to notice.** [[036-parallel-graph-bfs]] and
 [[046-ticket-reservation]] and [[045-llm-batch-dispatcher]] all shipped a boilerplate that
 **passed** their own correctness suite, a real, unguarded data race that simply didn't
 happen to corrupt the specific values those particular runs checked. Only running the exact
 same test under ThreadSanitizer revealed the defect. If this course's own authoring had
 stopped at "the tests pass," three shipped questions would have quietly graded a race as
 correct.
- **ThreadSanitizer finds *missing synchronization*, not *insufficient ordering*.**
 [[013-store-buffering]] and [[014-atomic-fences]] both involve atomics with a genuinely too-weak
 memory order, no data race by the language's technical definition, since every access is
 atomic, so TSan has nothing to report on either file. Those questions could only be
 verified by direct measurement: thousands of trials, counting how often a physically
 provable-impossible outcome occurred. Different bug shape, different tool, and no amount of
 running the *first* tool longer would ever have caught it.
- **A hang is a distinct failure mode from a wrong answer or a flagged race, and it needs its
 own catch.** [[019-fizzbuzz-multithreaded]], [[021-dining-philosophers]], and
 [[044-robot-grid-locking]] all have naive versions that deadlock, the process never
 crashes, never prints a wrong value, it just stops making progress. A test with no timeout
 hangs *with* the bug, forever, and never reports anything. This platform's runner times out
 every stage specifically because "the test never finished" has to be a distinguishable,
 reportable outcome, not silence.
- **A race can be real and still not show up in a small number of trials.** The 13,071
 violations measured above are what heavy, sustained contention (8 threads, thousands of
 attempts) looks like; a lighter test, two threads, ten iterations, might
 easily complete with zero observed violations purely by luck, and "zero violations in ten
 tries" proves nothing about a bug whose failure window is a handful of nanoseconds.
 [[025-thundering-herd]]'s wasted-wakeup counter and this question's `violations` counter
 both exist for the same reason: turn "did it fail" (a coin flip, at low trial counts) into
 "how many times did it fail out of a known-large number of attempts" (a number that's
 either zero, reliably, or clearly not).
- **Even multiple runs of the *same* build can miss a bug that a perturbed build catches.**
 This platform's shaken-stress stage (`-DSHAKE`, random delays at marked points) exists
 because compiling and running the identical binary repeatedly tends to hit the same handful
 of schedules the CPU already prefers, this course's own `check` script found a two-mutex
 deadlock that passed three clean plain-TSan runs and only hung on the twelfth *shaken* run.

## 7. Interview follow-ups

**"Given a piece of concurrent code and no test suite, how would you decide it's actually
correct?"** State the invariant explicitly first, this course's axiom is "no mutex without a
stated invariant, and which threads can observe it broken", then design a test that directly
observes a violation of that specific invariant (a `violations` counter, an address-layout
check, a wasted-wakeup count), not just a final-answer comparison; run it under TSan for
missing synchronization; run it under a scheduling-perturbation tool (or just many, many
trials) for a schedule-dependent bug the CPU doesn't happen to produce on its own; and treat
a hang as a distinct, must-be-detected outcome, not an absence of failure.

**"Why doesn't 'it passed code review' substitute for any of this?"** Because the entire
point of this question's boilerplate is that it reads correctly, a reviewer checking "does
this look like a spinlock" would very plausibly approve `while (locked_) {} locked_ = true;`
on sight. The bug is only visible as a *property of concurrent execution*, which static
reading of source code structurally cannot simulate; this is precisely why dynamic tools
(TSan) and adversarial scheduling (stress/shake) exist as a category, not as a substitute for
review but as the only way to observe what review cannot.

**"You have a rare, hard-to-reproduce concurrency bug reported from production, how do you
even start?"** Reproduce it under load, not under casual testing, this reading's own
13,071-violations number came from 8 threads doing 16,000 total attempts, not from running
the buggy code twice by hand. Once you have a workload that reproduces it reliably (even if
rarely per-attempt, reliably across many attempts), TSan and a debugger both become useful;
before that, you're debugging a coin flip.
