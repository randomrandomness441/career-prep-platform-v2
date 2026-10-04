## 1. Reframe the problem

A resource pool with a handful of slots, and a crowd of threads waiting for one. The
instinct, once you've built the `acquire`/`release` pair with a mutex and a condition
variable, is to notify with `cv.notify_all()` on every release, it's the safe-looking
choice. Nobody gets left behind; every sleeping thread gets a chance to check.

That instinct is where the name comes from. `release()` opens exactly *one* slot. If a
hundred threads are asleep waiting for a slot, `notify_all` wakes all hundred, every one of
them has to re-acquire the shared mutex (serializing them against each other), recheck the
predicate, discover somebody else already took the slot, and go back to sleep. Ninety-nine
threads did real work, a lock acquisition, a context switch back into the kernel, to
learn nothing. That's a thundering herd: a stampede of wakeups where only one event
occurred.

The fix reuses a rule from [[017-print-foobar]]: *`notify_one` is safe when every waiter is
interchangeable.* Here every thread waiting on the pool is checking the exact same
predicate, "is a slot free?", so there is no "right" thread to wake and no risk of picking
wrong. Waking one is not a gamble, because whichever thread the OS happens to choose will
find the predicate true, since the release that woke it is what made it true.

## 3. The broken version, first

The broken version differs from the correct one in exactly one call:

```cpp
cv_.notify_all(); // wakes every waiter, for a slot only one of them can take
```

**Why it looks right:** it's the conservative choice, nobody is silently left asleep,
which is exactly the failure mode ([[019-fizzbuzz-multithreaded]]'s lost wakeup) that
`notify_all` is usually the fix *for*. It's easy to internalize "when in doubt, notify_all"
as a blanket safety rule without noticing that safety and efficiency are different axes, and
this design is safe either way, every acquire eventually succeeds, no thread is stranded.
The bug here isn't a hang or a wrong answer; it's waste, and waste doesn't show up by
reading the code.

Instrumented and run, release one slot at a time to 100 waiting threads, counting every
wakeup that found nothing to do:

```
1936 wasted wakeups releasing 100 slots one at a time to 100 waiters (budget: 30)
```

Change the one call, `cv_.notify_one()`, same 100 threads, same sequence of releases:

```
bounded pool respects capacity, and releasing one slot at a time to 100 waiters wastes no wakeups
```

Zero. Every notification woke exactly the thread that could proceed, because every
sleeper's predicate was identical, there was never a "wrong" thread to wake in the first
place.

## 6. Where this solution fails

- **`notify_one` stops being correct the moment waiters aren't interchangeable.** If a
 future version of this pool let callers request slots tagged by type (e.g. read-only vs.
 read-write connections), a released read connection woken by `notify_one` might hand the
 wakeup to a thread that only accepts a write connection, it rechecks, fails, sleeps again,
 and the actual eligible waiter was never told. That's [[019-fizzbuzz-multithreaded]]'s
 lost-wakeup bug arriving through a different door: `notify_one` is a bet that whichever
 thread wakes can use what's available, and it's only a safe bet while every waiter's
 predicate is the same.
- **Multiple slots freed close together can still under-notify with a single `notify_one`
 per release.** This solution calls `notify_one` exactly once per `release()`, which is
 correct here because each `release()` opens exactly one slot, but a batched API
 (`release_many(k)`) would need `k` wakeups, or a `notify_all` for that one call, not a
 single `notify_one`. Blindly always calling `notify_one` regardless of how much became
 available reintroduces a lost-wakeup risk in the opposite direction.
- **A spurious OS-level wakeup still costs a `wasted_wakeups` increment even with
 `notify_one`.** The standard permits `condition_variable::wait` to return with no
 notification at all. It's rare in practice (the measurement above saw zero across every
 run), but "zero" is empirical, not guaranteed, the predicate-recheck loop exists
 specifically because the standard doesn't promise otherwise.
- **This doesn't fix contention, only wasted wakeups.** All hundred threads still queue on
 the same mutex to check the same predicate; `notify_one` reduces *how many* threads make
 that trip per release, not the fact that they all share one lock. At extreme waiter
 counts (thousands, not hundreds) the mutex itself becomes the bottleneck regardless of
 which notify call you use, sharding the pool into independent sub-pools, each with its
 own mutex and condition variable, is the next lever once this one is exhausted.

## 7. Interview follow-ups

**"Why does the CPU spike under `notify_all` here, specifically, what's actually
running?"** Every woken thread does real kernel work before finding out it lost: a
scheduler wakeup, a mutex lock (which serializes all hundred against each other one at a
time, since only one can hold the mutex at once), a predicate check, an unlock, and back to
sleep. Measured here: 1936 of those round trips for 100 sequential releases where at most
100 could ever be useful (one per release), the other ~1836 are pure overhead, and they all
funnel through the same single mutex, so the "spike" is really a hundred threads briefly
queuing for a lock that only one of them needed.

**"The guide suggests a LIFO queue instead of FIFO to reduce contention under load, does
that apply here?"** That's a different lever: it addresses which thread among many
*ready-to-run* threads the OS schedules next (LIFO favors a thread whose stack and cache
state are still warm), not how many threads get woken in the first place. It would shave
some cache-miss cost off of whichever thread's wakeup *is* useful; it does nothing about the
other 1836 wasted ones. `notify_one` and scheduling policy are solutions to different parts
of the cost.

**"Extreme load, 10,000 waiters on this same pool, slots released one at a time. What
breaks first?"** Even with `notify_one`, all 10,000 threads are asleep on the *same*
condition variable and mutex, so a burst of releases still means each one briefly locks that
shared mutex. The mutex itself, not the notify choice, becomes the ceiling, the fix at that
scale is sharding the pool (multiple sub-pools, each with independent slots, mutex, and
condition variable), so releases and acquisitions on different shards never contend at all.

**"Production ops, how would you tell if a live service has this exact bug, without
access to the source?"** Watch context-switch rate or `futex` syscall counts (on Linux,
`perf stat -e context-switches`) against request/release rate, a thundering herd shows as
context switches scaling with the *number of waiters*, not with the number of *events*
(releases). A healthy `notify_one` design has switches roughly tracking the release rate;
a `notify_all` design's switch count grows with however many threads happen to be queued
at the time, which is a very different, and much noisier, signal on a dashboard.
