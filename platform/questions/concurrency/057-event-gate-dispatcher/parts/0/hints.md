Start from the trap, not the fix. The obvious first draft checks "is an event in
progress?" and then decides what to do — as two separate steps:

```cpp
void reg_cb(std::function<void()> cb) {
    if (event_active_) { pending_.push_back(std::move(cb)); }   // step 1: check
    else                { cb(); }                                // step 2: act
}
```

What can happen between step 1 and step 2? `end_event()` can run in that gap — flip
`event_active_` to false and drain `pending_` — before this thread's `push_back` lands.
The callback gets pushed into a waiting list that has *already been drained and won't be
looked at again*. It's not corrupted, not duplicated — it's just gone, forever, and
nothing ever tells you.
---
The fix is to make "check the flag" and "act on it" a single atomic step, under one lock
— and to hold that *same* lock across `end_event()`'s own flag-flip-and-drain, so the two
can never interleave:

```cpp
void reg_cb(std::function<void()> cb) {
    std::unique_lock<std::mutex> lk(m_);
    if (event_active_) {
        pending_.push_back(std::move(cb));
        return;                     // still under the lock -- the push is guaranteed
                                     // to land before or after end_event()'s drain,
                                     // never "during" it
    }
    lk.unlock();
    cb();                           // run OUTSIDE the lock
}
```

Two things to notice: the lock is released *before* calling `cb()` in the no-event case,
and `end_event()` needs to do the same for the whole queued batch.
---
```cpp
void end_event() {
    std::vector<std::function<void()>> to_run;
    {
        std::lock_guard<std::mutex> lk(m_);
        event_active_ = false;
        to_run.swap(pending_);      // empties pending_ in one O(1) move, under the lock
    }
    for (auto& cb : to_run) cb();   // run every queued callback OUTSIDE the lock
}
```

Why run callbacks outside the lock? Two reasons, both real: holding the lock across
arbitrary user code means every `reg_cb` call from every other thread blocks for the
entire batch, not just for the bookkeeping — and if any callback calls `reg_cb` itself
(reentrant registration, which the interface doesn't forbid), calling it while still
holding `m_` is an instant self-deadlock on a non-recursive mutex. Copy the work out
under the lock, then do the work with the lock released — the exact same "copy under
the lock, invoke outside it" shape as [[025-thundering-herd]]'s and any observer-list
pattern.
