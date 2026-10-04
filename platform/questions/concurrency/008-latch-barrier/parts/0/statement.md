# Latches, Barriers and a Phased Simulation

## ELI5: a group hike with checkpoints

A group of hikers is doing a multi-day trail with checkpoints along the way. The rule:
**nobody starts the next leg of the trail until every single hiker has reached the
current checkpoint.** The fastest hiker doesn't get to just keep walking, they wait.
Once everyone's arrived, the trip leader tallies up the group's total distance for that
leg, and only *then* does anyone set off on the next one.

This keeps the group moving in lockstep, phase by phase, with one clean tally happening
between each phase, never while someone's still mid-hike.

## What you're actually building

```cpp
// Runs `n_phases` phases across `n_workers` threads.
//
// compute(w, p) called once per worker per phase, on worker w's own
// thread, and returns that worker's contribution.
//
// on_phase_end(p, total) called exactly ONCE per phase, after every worker has
// returned from compute(*, p) and before any worker
// enters compute(*, p+1). `total` is the sum of that
// phase's n_workers contributions.
//
// Returns the totals, one per phase, in phase order.
std::vector<long long> run_phased_simulation(
int n_workers, int n_phases,
const std::function<long long(int worker, int phase)>& compute,
const std::function<void(int phase, long long total)>& on_phase_end);
```

Each hiker is a worker thread; each leg of the trail is a phase; `compute` is one hiker
walking their leg; `on_phase_end` is the trip leader's tally at the checkpoint.

## Requirements

1. Exactly `n_workers` threads (hikers), each running every phase (leg) in order.
2. **The checkpoint is a hard wall.** Nobody crosses it early, not by one instruction,
 not "I'm basically done anyway."
3. `on_phase_end` (the tally) runs exactly once per phase, at a moment when **no hiker is
 still mid-leg**, and it sees that phase's complete set of contributions, nobody's
 distance is missing, nobody's counted twice.
4. The returned vector has `n_phases` entries, and `totals[p]` is phase p's sum.
5. `n_workers <= 0` or `n_phases <= 0` returns an empty vector without spawning anything
 , no hikers, no hike.

## Why the constraints exist

- **Use `std::barrier`.** The whole point of this question is the reusable checkpoint-
 with-a-tally-function primitive; hand-rolling your own counter is the version you're
 replacing with something better.
- **The tally function must not throw.** `std::barrier` calls `std::terminate` if it
 does, there's no "the trip leader panics and the whole hike ends in chaos" recovery
 path.

**A tooling note, so the harness doesn't lie to you.** `std::barrier` guarantees that
everything a worker did before reaching the checkpoint is visible to the tally function,
no atomics required, by the standard's own rules. That guarantee is real, but on this
machine ThreadSanitizer can't actually see it: libc++ implements the barrier algorithm
inside the system dylib, which isn't instrumented, so TSan misses the happens-before edge
and reports a false race on any *plain* variable you pass through the barrier. Keep the
per-worker results in `std::atomic` and the tooling will agree with you. Section 6 of the
reading has the measurement.
