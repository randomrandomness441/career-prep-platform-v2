Pack `tick` and `sequence` into a single `std::atomic<std::uint64_t>` — not two separate
atomics. Two separate atomics (`last_tick_`, `seq_`) means "is this a new tick?" and
"reset the sequence" are two different operations on two different variables, with a gap
between them exactly like every other TOCTOU bug in this course: two threads can both
observe "yes, new tick" before either one resets `seq_`, and both end up computing
`sequence = 0` for the new tick independently. Two threads, same tick, same sequence,
same ID.
---
Shape of the fix — a compare-and-swap retry loop, same pattern as
[[015-treiber-stack]]'s push/pop:

```cpp
std::uint64_t old_state, new_state, tick, seq;
do {
    old_state = packed_.load();
    std::uint64_t old_tick = old_state >> kSeqBits;
    std::uint64_t old_seq  = old_state & kSeqMask;
    std::uint64_t now = clock_();

    if (now > old_tick) {
        tick = now; seq = 0;
    } else {
        tick = old_tick; seq = old_seq + 1;
        // TODO: what happens when seq overflows kSeqMask?
    }
    new_state = (tick << kSeqBits) | seq;
} while (!packed_.compare_exchange_weak(old_state, new_state));
```

The key property: every retry re-reads `old_state` fresh and recomputes `tick`/`seq`
from *that* read. If another thread's CAS lands first, this thread's CAS fails, the loop
retries, and it recomputes against the new, current state — never against stale data.
---
Sequence overflow: if `seq` would exceed `kSeqMask` (4095), you're out of numbers for
this tick before the clock has actually moved on. Force it forward yourself:

```cpp
if (seq > kSeqMask) { tick = old_tick + 1; seq = 0; }
```

This is exactly why `clock_()` alone isn't enough state to track — the generator's own
notion of "current tick" can run *ahead* of what the injected clock reports, under
enough load, and has to win that comparison from then on until the real clock catches up
(`now > old_tick` naturally stops being true once you're ahead, and you correctly fall
into the "same tick, bump sequence" branch using your own advanced tick instead).
---
Finally, pack `worker_id` into the *returned* value, not into the atomic being CAS'd —
`worker_id` never changes for a given generator, so there's nothing to synchronize about
it:

```cpp
return (tick << (kSeqBits + kWorkerBits)) | (worker_id_ << kSeqBits) | seq;
```
