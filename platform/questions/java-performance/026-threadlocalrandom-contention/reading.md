## 1. Reframe

A completely correct, thread-safe class can still be a severe bottleneck the moment
multiple threads genuinely compete for its shared mutable state — correctness and
scalability are different properties, and `Random` has the first without the second.

## 2. What was actually measured, for real

JDK 21, this machine, 8 threads each calling `nextInt()` 20 million times:

```
shared Random,       8 threads: 51090ms
ThreadLocalRandom,   8 threads:    71ms
```

About 719x. This is one of the largest gaps measured anywhere in this pack —
larger than the megamorphic dispatch cliff, larger than the ReDoS-adjacent findings —
because CAS-retry collapse under real multi-threaded contention compounds in a way few
of this pack's other findings do.

## 3. The broken version, first

A shared `Random` field, injected once and reused everywhere, is an extremely common,
completely reasonable-looking pattern in single-threaded or lightly-threaded code. The
same pattern, moved into a highly concurrent service (many request-handling threads all
touching the same injected `Random`), turns a correct design decision into a severe
throughput bottleneck with zero change to the surrounding logic — the cost is entirely a
function of how many threads actually contend for it in production.

## 4. Interview follow-ups

- Does this same contention shape apply to any shared object using a CAS-based update
  internally, or is `Random` special? It applies generally — any shared mutable state
  updated via CAS (a hand-rolled counter, a custom cache using compare-and-swap) can
  exhibit the same retry-storm collapse under enough concurrent contention. `Random`
  is simply a common, easy-to-miss example because it looks like an immutable,
  stateless-feeling utility rather than obviously mutable shared state.
- `LongAdder` (question 005) fixes contended counting by striping state across
  multiple cells instead of one shared field. Could a similar striping approach fix
  contended `Random` usage, instead of switching to `ThreadLocalRandom`? In principle,
  yes — but `ThreadLocalRandom` already *is* that fix, purpose-built: it gives each
  thread its own independent generator state rather than striping one generator's state
  across cells, which is a cleaner solution for something that's naturally
  per-thread-independent rather than needing to be globally summed like a counter.
