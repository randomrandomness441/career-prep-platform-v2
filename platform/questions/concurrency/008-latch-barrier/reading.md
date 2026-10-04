## 1. Reframe the problem

"Wait until everybody is done" sounds like one idea. It is two, and mixing them up is the
bug in this question.

**One-shot: `std::latch`.** A counter that only goes down. Threads call `count_down()`;
threads call `wait()` and stay there until the counter reaches zero. It cannot be reset,
reused, or increased, once it hits zero it is open forever.

```cpp
std::latch ready(3);
// three worker threads: ...set up...; ready.count_down();
ready.wait(); // the main thread: proceed once all three are set up
// ready.arrive_and_wait() == count_down() then wait(), for a participant
```

That is the right tool for start gates ("nobody begins until every thread exists") and for
finish gates ("the caller resumes once the last result lands"). It is a gate you open once.

**Reusable: `std::barrier`.** A gate that closes again behind you. Constructed with the
number of participants; each phase, every participant calls `arrive_and_wait()`; when the
last one arrives the barrier runs an optional **completion function** and only then releases
everybody and resets itself for the next phase.

```cpp
std::barrier sync(n, [&]() noexcept { aggregate(); }); // runs between phases
// each worker, each phase: do_my_part(); sync.arrive_and_wait();
```

Now the reframe. The problem is not "wait for everyone", you can write that with a counter
in five minutes. The problem is **reuse**. A phase gate has to open, let everybody through,
and be shut again before the fastest of them comes back around, and the thing that closes it
is running on the same threads it is supposed to be blocking. Every hand-rolled attempt dies
somewhere in that reset: either the counter is not cleared, or it is cleared by a thread
that has already left, or a fast worker starts counting for phase p+1 while a slow one is
still waiting to be released from phase p.

That is why `std::barrier` exists as a primitive rather than a recipe. And the completion
function is not decoration: it is the only place in the whole program where you *know*
every worker is parked, so it is the only place an aggregation across all workers can read
their results without a lock and without a race.

## 3. The broken version, first

Four workers, six phases. In phase *p*, worker *w* contributes `(w+1) * (p+1)`, so the phase
total should be `10 * (p+1)`. The synchronisation is the obvious hand-rolled one, count the
arrivals, let the last one in aggregate and wake everybody:

```cpp
void end_of_phase(int p) {
    std::unique_lock<std::mutex> lk(m);
    ++arrived;
    if (arrived >= N) { // last one in
        long long total = 0;
        for (int w = 0; w < N; ++w) total += slots[w].load();
        totals[p] = total;
        cv.notify_all();
    } else {
        cv.wait(lk, [] { return arrived >= N; });
    }
}
```

Each worker also records how many phases it has completed, so the program can print how far
apart the workers drifted. Worker *w* sleeps `20*w` microseconds per phase, so worker 0 is
the fast one. Actual output:

```
phase total expected
 0 10 10
 1 31 20
 2 42 30
 3 49 40
 4 56 50
 5 60 60
worst observed spread between workers: 5 phases
```

Phase 0 is right and everything after it is garbage. **Worker 0 finished the entire
six-phase simulation while worker 3 was still on phase 1.** The barrier stopped existing
after the first phase.

The cause is one line: `arrived` is never reset. After phase 0 it sits at 4, so in phase 1
the very first worker to arrive already satisfies `arrived >= N`. It does not wait. It
aggregates whatever happens to be in `slots` at that instant, a mix of phase 1 values from
the workers that got there and phase 0 leftovers from those that did not, which is where 31
comes from, and walks straight into phase 2.

Notice what this failure is *not*. It is not a data race: every access goes through the
mutex or an atomic, and ThreadSanitizer has nothing to say about it. It is not a crash. It
is a program that runs to completion, quickly, and produces numbers that look plausible if
you do not know what they should be. The only thing wrong is that a phase boundary was not a
boundary.

And the obvious patch does not work either. Reset the counter when the last worker arrives,
`if (arrived >= N) { arrived = 0; cv.notify_all(); } else cv.wait(lk, []{ return arrived == 0; });`
, and the fast worker gets released, finishes the next phase, and increments `arrived` back
to 1 before a slow worker has even woken up. The slow one re-checks its predicate, sees
`arrived != 0`, and sleeps for good. Actual output, 200 phases requested, printed after a
two-second watchdog:

```
after 2 seconds, phases completed per worker: 2 1 1 2 (expected 200 each)
```

Identical on every run. You have traded wrong answers for a hang, which at least is loud.
Getting this right needs a generation number and a careful argument about which thread may
advance it and when. That argument is what `std::barrier` has already had, once, correctly.

Same program, same sleeps, `std::barrier` with the aggregation in the completion function:

```cpp
auto aggregate = []() noexcept {
    long long total = 0;
    for (int w = 0; w < N; ++w) total += slots[w].load();
    totals[phase.fetch_add(1)] = total;
};
std::barrier sync(N, aggregate);
// worker w, each phase: slots[w].store((w+1)*(p+1)); sync.arrive_and_wait();
```

Actual output:

```
phase total expected
 0 10 10
 1 20 20
 2 30 30
 3 40 40
 4 50 50
 5 60 60
worst observed spread between workers: 1 phases
```

The spread of 1 is not slop, it is the exact guarantee: while worker *w* is in phase *p*,
another worker has either completed *p* phases (it is in phase *p* too, or about to enter
it) or *p+1* (it finished and is parked at the barrier). Never 2. The tests in this question
check precisely that inequality on every call, which is why they catch the broken version
deterministically rather than one run in fifty.

## 6. Where this solution fails

- **A participant that does not arrive hangs everyone, forever.** There is no timeout and no
 cancellation. Measured: three workers, one throws out of its loop in phase 2, after two
 seconds the program had completed 8 of 15 worker-phases and was permanently stuck. Any
 `compute` that can throw must be wrapped, and the wrapper must still arrive
 (or call `arrive_and_drop()` to leave the group for good and let the rest continue).

- **The completion function must not throw.** `std::barrier` calls `std::terminate` if it
 does. That includes anything it calls, and in this exercise it calls a user-supplied
 callback, which is exactly the kind of code that grows a `throw` two years later.

- **The completion runs on an arbitrary thread, and which one changes every phase.**
 Measured over 50 phases with 4 workers: all 4 threads ran it at least once. So it must not
 touch thread-local state, must not assume it is on the "main" thread, and must not do
 anything that needs a specific thread (UI work, a per-thread allocator's bookkeeping, a
 thread-affine handle).

- **Everything waits for the slowest worker, every single phase.** The barrier turns your
 throughput into `n_phases × (slowest worker per phase)`. Uneven work per worker is no
 longer averaged out over the run, the imbalance is paid again at every boundary. If the
 phases are short, the barrier itself dominates and the parallel version loses to the
 serial one. This is the reason real bulk-synchronous code fights so hard for balanced
 partitions.

- **The barrier counts arrivals; it has no idea what a phase is.** If one worker calls
 `arrive_and_wait()` a different number of times than the others, an early `continue`, a
 `break` out of the loop, a phase skipped on a special case, the barrier will happily pair
 up arrivals from different logical phases and report nothing. The phase alignment is your
 invariant to maintain, not something the type checks.

- **The participant count is fixed at construction.** No adding a worker mid-run.
 `arrive_and_drop()` only shrinks. A pool that resizes needs a new barrier, and handing a
 new barrier to threads that are currently blocked on the old one is its own problem.

- **`std::latch` cannot do this job at all.** It has no reset. Using latches per phase means
 constructing `n_phases` of them up front and keeping them all alive, and the moment the
 phase count is dynamic that falls apart. Latch for one-shot gates, barrier for phases,
 reaching for the wrong one is not a style question, it is a redesign.

- **ThreadSanitizer cannot see through the barrier on this platform.** Measured with a
 twelve-line program: two threads, one plain `long long` per thread, a barrier, and a
 completion function that sums them. That program is correct by the standard, the arrival
 happens-before the completion, and TSan reports a data race on it every run. libc++
 implements the barrier algorithm inside the system dylib, which is not instrumented, so
 the happens-before edge is invisible and TSan assumes the worst. Making the shared slots
 `std::atomic` restores a clean run. Worth knowing beyond this exercise: **a sanitizer's
 silence is only as good as its view of your synchronisation**, and any primitive whose
 guts live in an uninstrumented library is a blind spot.

- **Nothing here bounds memory or handles backpressure.** Every worker's contribution for
 phase *p* is overwritten in phase *p+1*. If the aggregation needs to keep per-phase
 history, this exercise keeps one number per phase, that history grows with the phase
 count, and a long-running simulation needs a policy for it.

## 7. Interview follow-ups

**"You said 'never 2' phases of spread is an exact guarantee, not an approximation, why
can't the spread ever reach 2?"** By construction: the barrier only releases phase *p* once
every participant has arrived for it, and nobody can begin phase *p+1*'s work until they've
been released from *p*. So at any instant, every worker is either still working on the same
phase as everyone else, or has finished it and is parked waiting at the barrier for the
slowest one, there's no way for one worker to be two full phases ahead, because the barrier
itself is the thing physically preventing it from starting phase *p+2* before phase *p+1*
has even been released for everyone.

**"The reset-the-counter patch turned wrong answers into a hang instead. Walk through why
that specific race happens."** The fast worker, released when the counter hits N and gets
reset to 0, immediately starts its next phase's work and increments the counter again, 0 to
1, *before* a slow worker has even woken up from its `cv.wait()` for the previous reset.
That slow worker's predicate re-check then sees `arrived != 0` (because the fast worker just
bumped it), concludes it's not time yet, and goes back to sleep, permanently, since nothing
will ever wake it for a check that would now succeed. The fundamental issue: one shared
counter can't represent "how many have arrived for phase p" and "how many have arrived for
phase p+1" at the same time, and a naive reset conflates the two.

**"You measured that TSan reports a false race on a correct barrier-based program. How would
you convince a teammate the flagged race is a false positive and not a real bug, without
just asserting it?"** Reason from what TSan can and can't see: it instruments code it can
observe the source of, but `std::barrier`'s actual synchronization on this platform's libc++
lives inside an uninstrumented system dylib, TSan has no visibility into the
happens-before edge the barrier genuinely establishes, so it falls back to assuming none
exists. The convincing argument isn't "trust me," it's demonstrating that making the shared
data `std::atomic` (adding synchronization TSan CAN see, even though the barrier already
made it logically unnecessary) makes the warning disappear, which shows the flagged access
really was safe, just invisible to the tool, not that the tool is generally unreliable.

**"Small machine, 2 cores, 8 workers using this barrier. What actually happens?"** The
barrier's correctness is unaffected, arrival counting and phase release work identically
regardless of how many workers are genuinely running in parallel versus time-sliced on 2
cores. What changes is throughput: with only 2 cores for 8 logical workers, most of them are
waiting for CPU time rather than for each other, and the barrier's "every phase waits for
the slowest worker" cost compounds with scheduling latency on top of actual work imbalance,
the phase boundary now also has to wait for the OS to get around to running whichever worker
hasn't been scheduled yet.

**"Production ops, how would you monitor a long-running barrier-synchronized pipeline to
catch a worker that's silently falling behind, before it becomes a full stall?"** The
barrier's own guarantee caps the phase *spread* at 1 by construction, so spread alone can't
signal trouble building up, since it never gets a chance to exceed that before everyone else
is already blocked waiting. The signal that actually catches this early is *time parked at
the barrier per phase*, per worker: a worker that's routinely the last to arrive, with a
growing gap between its arrival time and everyone else's, is the one about to become a full
stall, tracking that trend identifies the straggler before the pipeline visibly stops
making progress, not after.
