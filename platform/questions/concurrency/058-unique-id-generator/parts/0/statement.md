# A Fast, Thread-Safe Unique ID Generator

## ELI5: a ticket machine that also stamps the time and the counter it came from

Think of the numbered-ticket dispenser from [[025-thundering-herd]] and
[[052-ordered-disk-log]]. Now picture several different service instances sharing it at
once. Every ticket needs three things baked into it: roughly *when* it was printed,
*which machine* printed it, and *which ticket number within that instant* it was. Two
tickets from two different machines, printed in the same split second, must never look
identical. That's easy, because the machine number is stamped right on them. And one
machine printing thousands of tickets a second must never hand out the same ticket
twice, no matter how many people are pulling tickets from it at once.

That's a real production problem. Many services generate IDs independently, and there's
no central coordinator to ask "has anyone used this ID yet?" There can't be. Asking a
central coordinator on every single ID would become the exact bottleneck this design
exists to avoid.

## What you're actually building

```cpp
class UniqueIdGenerator {
public:
    // worker_id identifies which service/instance this generator belongs to.
    // clock returns the current "tick" -- stands in for milliseconds-since-epoch,
    // and is injectable so tests (and you) can control time deterministically
    // instead of racing against the real wall clock.
    UniqueIdGenerator(std::uint64_t worker_id, std::function<std::uint64_t()> clock);

    // Packs (tick, worker_id, sequence-within-this-tick) into one uint64_t:
    //   bits 63..22 = tick     bits 21..12 = worker_id     bits 11..0 = sequence
    std::uint64_t next_id();
};
```

Every ID is `(tick << 22) | (worker_id << 12) | sequence`. `worker_id` is 10 bits
(0-1023). `sequence` is 12 bits (0-4095), and it resets to 0 every time the tick
advances.

## Requirements

1. `next_id()` is safe to call concurrently from any number of threads on the same
   generator. No two calls on the same instance ever return the same ID.
2. **Two different generator instances, with different `worker_id`s, never collide with
   each other.** Different machines, different tickets, always.
3. If more than 4096 calls land within the same tick, the generator must not wrap the
   sequence back to 0 and silently collide. Instead it advances its own notion of the
   current tick past what the clock is reporting, the same way a busy ticket machine
   that's run out of numbers for "this second" just starts stamping the *next* second
   early, and continues from sequence 0 there.
4. IDs from one generator are non-decreasing for any two calls where one genuinely
   happens after the other. That means the usual happens-before sense: joined threads,
   observed side effects. Not just "issued microseconds apart on different cores."

## Why the constraints exist

**Build this as a single atomic compare-and-swap retry loop on a packed
`(tick, sequence)` pair.** It's the same shape as [[015-treiber-stack]]'s push and pop,
just retrying on two packed numbers instead of a pointer. That keeps "is this a new
tick" and "reset the sequence" as one indivisible decision instead of two separate
operations with a gap between them, and that gap is the actual bug this question is
about. Whether the CAS loop is also *faster* than a plain mutex here is a separate
question. Measure it, don't assume it. See reading section 5, and don't be surprised if
the honest answer depends on how many threads are actually contending.
