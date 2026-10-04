## 1. Reframe the problem

This question is [[004-interface-races]] wearing a different costume. There, the bug was
"check if the stack is empty, then act on it" as two separate lock acquisitions, with
another thread's `pop()` slipping into the gap. Here it's "check if an event is in
progress, then act on it" as two separate lock acquisitions, with another thread's
`end_event()` slipping into the gap. Same shape, same fix: the check and the act have to
be one indivisible step, under one lock, or the world can change out from under you
between them.

What makes this version harder than 004 is what's on the other side of the boundary:
not a single element coming out of a container, but an entire batch of arbitrary,
caller-supplied callbacks that need to run, and running them is exactly the kind of
"arbitrary code" you should never do while holding a lock, which opens up a second,
completely different failure mode layered on top of the first.

## 2. The tools, from scratch

**A "gate" is just a flag plus a place to put the things that arrive while it's
closed.** `event_active_` is the flag; `pending_` is the waiting list. Every operation
that reads or writes either one has to go through the same mutex, or the flag and the
list can disagree with each other about what state the gate is in.

**"Check the flag, decide what to do" has to be one critical section, not two.**
```cpp
{
    std::lock_guard<std::mutex> lk(m_);
    if (event_active_) { pending_.push_back(std::move(cb)); return; }
}
cb();
```
If you instead read the flag under the lock, release it, *then* decide, that's the bug.
Whatever the flag said is stale the instant the lock is released.

**Copy the work out, then do the work with the lock released.** `end_event()` needs to
flip the flag and grab everything in `pending_`, both under the lock, and then invoke
each callback with the lock already released:
```cpp
std::vector<std::function<void()>> to_run;
{ std::lock_guard<std::mutex> lk(m_); event_active_ = false; to_run.swap(pending_); }
for (auto& cb : to_run) cb();
```
`swap` (not a copy) empties `pending_` in O(1), under the lock, in the same breath as
flipping the flag, so there's no window where the flag says "not active" but the
waiting list still has stale entries in it, and no window where a fresh `reg_cb` could
land in a `pending_` that's about to be swapped out from under it.

## 3. The broken version, first

Here's the version that reads correctly:

```cpp
void reg_cb(std::function<void()> cb) {
    bool active;
    { std::lock_guard<std::mutex> lk(m_); active = event_active_; }
    if (active) {
        std::lock_guard<std::mutex> lk(m_);
        pending_.push_back(std::move(cb));
    } else {
        cb();
    }
}
```

Run it: one thread calls `begin_event()`, then a second thread calls `reg_cb(cb)` and is
scheduled out for a moment right after reading `active = true` but before it re-acquires
the lock to push. The first thread calls `end_event()` in that gap, it acquires the
lock, sees `pending_` is still empty (the second thread hasn't pushed yet), and returns.
The second thread now wakes up, re-acquires the lock, and pushes `cb` into `pending_`,
a waiting list nobody will ever drain again.

```
trial 0: callback ran 0 times (expected 1)
trial 1: callback ran 0 times (expected 1)
trial 2: callback ran 0 times (expected 1)
trial 3: callback ran 0 times (expected 1)
trial 4: callback ran 0 times (expected 1)
```

5 out of 5, with the gap widened to make the point, but the same interleaving is
available on any real schedule, just rarer. **And ThreadSanitizer says nothing at all
about it**, every individual access to `event_active_` and `pending_` is correctly
locked; there is no unsynchronized memory access anywhere in this program. The bug lives
entirely in the gap *between* two individually-correct critical sections, which is
precisely the class of bug a race detector cannot see, because nothing about it is a
data race. This is the same lesson [[004-interface-races]] opens with, "no data race,
ThreadSanitizer finds nothing", and it's worth re-learning here because it's easy to
start trusting a clean TSan run as proof of correctness. It's proof of one *specific*
kind of correctness.

The second bug is a design smell rather than a crash-on-demand: running the queued
callbacks while `end_event()` still holds `m_` means every other thread's `reg_cb()`
call blocks for the whole batch, and a callback that calls `reg_cb()` itself (which the
interface never forbids) deadlocks outright on a non-recursive mutex.

## 4. Real-world usage

This exact shape, "gate closed during a maintenance/config-reload/failover window,
requests queue, gate reopens and drains them in order, requests after that pass straight
through", shows up anywhere a system needs a brief period where new work must wait for
an in-flight state transition to finish before it's safe to proceed. Database connection
pools do this around a schema migration; a distributed system's leader-election
transition queues incoming requests during the handoff rather than serving them against
a leader that might already be stale; a config-reload in a running service queues
callbacks that depend on the new config rather than letting them run against a
half-applied one.

It's the wrong tool when "wait for the event" isn't actually what callers want, if some
callers need to know *immediately* that an event is in progress rather than silently
queuing (a health check, say), forcing them through the same gate hides state they
needed to see.

## 5. Performance guarantees

**Holding the lock across callback dispatch is not a subtle cost, it's the difference
between microseconds and nothing.** Measured directly: `end_event()` draining 50 queued
callbacks at 20µs each (~1ms of total work) while another thread calls `reg_cb()` for an
unrelated, trivial callback at the same time:

```
lock held during dispatch: reg() blocked for ~1450-1490 us
lock released before dispatch: reg() blocked for ~0.2 us
```

Roughly **7,000x**. Every other thread's `reg_cb()` call, even ones that have nothing
to do with the batch currently draining, is serialized behind the entire batch's
runtime if the lock stays held. Releasing it before dispatch turns "blocked for the
whole batch" into "blocked for a `bool` check," because the only thing left to contend
on is the couple of instructions it takes to read the flag.

## 6. Where this solution fails

- **A callback that never returns.** `end_event()` runs the batch on the calling
 thread; one callback that blocks forever (waiting on I/O, deadlocked on something
 else) stalls every callback queued after it in the same batch, and stalls whichever
 thread called `end_event()`. This design has no per-callback timeout or isolation,
 a production version would likely dispatch each callback onto a thread pool instead of
 running the batch inline, trading "simple and ordered" for "resilient to one bad
 callback."
- **Extremely long queue windows.** If the event stays active for a long time under
 heavy registration traffic, `pending_` grows unbounded, there's no backpressure
 policy here, unlike [[024-backpressure]]'s explicit `overflow_policy`. A
 production version guarding against a pathological "event never ends" bug would need
 one.
- **Ordering across the reentrant case.** A callback that re-registers from inside
 `end_event()`'s drain runs immediately (the event is already marked inactive by then)
 rather than joining the *current* batch, which is correct per the stated contract,
 but worth being explicit about if an interviewer asks "what if the re-registered
 callback was supposed to also be part of this batch?" It isn't, and can't be, without
 changing the contract (see follow-ups).

## 7. Interview follow-ups

**"When can the same thread end up calling `reg_cb()` twice, effectively?"** (a fundamentals
question this exact interview reportedly asked) Two ways: directly, if the caller's own
code calls it twice back to back, nothing about the interface prevents that, and nothing
should; a user is allowed to register more than one callback. Or reentrantly, if a
callback passed to an earlier `reg_cb()` call itself calls `reg_cb()` when it runs (see
requirement/test 4 above), that's the case worth designing for deliberately, since it's
the one that can deadlock a naive implementation.

**"What's a concurrent modification exception, and does this design have that problem?"**
It's the classic Java/C# name for mutating a collection (like a `List`) while something
else is iterating it, typically detected at runtime and thrown as an exception in
managed languages. C++ has no such exception; the equivalent mistake here would be
iterating `pending_` in `end_event()` while another thread's `reg_cb()` pushes into it
concurrently, unguarded, undefined behavior, not a helpful diagnostic. The `swap`-under-
the-lock pattern sidesteps it entirely: nobody iterates `pending_` itself, only the local
`to_run` that already has exclusive, single-threaded access once the swap completes.

**"What are the possible deadlock scenarios here?"** Two, both already covered above: (1)
holding `m_` while invoking callbacks, combined with a callback that calls back into any
`EventGate` method, a hard requirement of "release the lock before calling arbitrary
code" for exactly this reason; (2) a callback that itself blocks forever on an unrelated
resource, which isn't a *lock* deadlock but has the same practical effect of stalling
`end_event()`'s caller indefinitely.

**"What is a mutex, in one sentence, and what invariant is `m_` protecting here?"** A
mutex guarantees at most one thread executes the code between `lock()` and `unlock()` at
a time. `m_` here protects the invariant "`event_active_` and `pending_` always agree
with each other", specifically, that no `reg_cb` call can observe `event_active_` and
then act on it after that observation has gone stale.
