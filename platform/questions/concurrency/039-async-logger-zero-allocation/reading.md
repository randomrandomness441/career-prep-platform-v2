## 1. Reframe the problem

The easy way to make a logger thread-safe is a mutex around a `std::queue<std::string>`,
lock, push, unlock. It's correct, and it's exactly what this question asks you not to build:
`log()` is meant to be called from code that cares about latency, and both halves of that
easy answer are expensive in ways that compound under load. A mutex means every producer
serializes against every other producer even though their messages have nothing to do with
each other. A heap-allocated `std::string` means every single log call takes a trip through
the global allocator, which very likely has its own internal lock, so you've paid for
contention twice, once for your own queue and once for memory you didn't need to ask for.

Reframe "make logging thread-safe" as "give every producer a way to claim its own private
piece of memory, with one atomic operation, and never make them wait on each other or on the
allocator." A pre-sized array of fixed records and one atomic counter does both: the counter
hands out distinct slots, and once a producer owns a slot, nothing else in the system is
touching it.

## 3. The broken version, first

The boilerplate is identical to the solution except for one type: `next_write_` is a plain
`std::size_t`, not atomic.

```cpp
std::size_t slot = next_write_++; // not atomic
```

**Why it looks right:** it's the most natural way to write "give me the next index" if you're
thinking about the logic in isolation rather than about what happens when two threads run
this exact line at the same instant, `x++` reads as a single, indivisible step, and for a
counter nobody else is touching that intuition is even correct. It's only wrong here because
many threads *are* touching it, concurrently.

Running it, 16 producer threads, 3,000 messages each:

```
drain() returned 46287 records, expected 48000 -- some messages were lost
(overwritten by another producer's claim on the same slot)
```

1,713 messages gone, every run. Two producers computed `next_write_`'s pre-increment value
as the same number, both got handed the same slot, and the second one to write simply
overwrote the first, there's no trace left that the first message ever existed. This is a
data race by the language's own definition (`++` on a non-atomic variable shared across
threads, with no synchronization), so it's also reliably visible to ThreadSanitizer, not just
to this counted-loss test.

The fix is `std::atomic<std::size_t>` and `fetch_add`:

```cpp
std::size_t slot = next_write_.fetch_add(1, std::memory_order_relaxed);
```

`fetch_add` is a single atomic read-modify-write, it returns the value from *before* the
add, and the standard guarantees that when N threads call it concurrently, they receive N
genuinely distinct values. Same 16-producer, 3,000-message run:

```
16 producers x 3000 messages each: no loss, no corruption, no duplicate
slots; a full buffer correctly refuses new entries
```

## 6. Where this solution fails

- **This is a fill-once buffer, not a real ring, reusing slots for a long-lived logger
 needs more machinery.** Once `capacity` is reached, every further `log()` call returns
 `false` forever; there's no reclaiming a slot after `drain()` has read it. A production
 logger that runs indefinitely needs actual wraparound (writing slot `seq % capacity`
 instead of `seq` directly), and that reintroduces exactly the recycling hazard
 [[038-multithreaded-memory-pool]]'s reading covers in depth, a slot the consumer hasn't
 finished reading yet must not be overwritten by a producer that's lapped it. This question
 deliberately sidesteps that by not wrapping; a real implementation can't.
- **`drain()` can return a "torn" batch across concurrent producers, and that's by design,
 not a bug**, it reads `next_write_` once at the start, so producers that claim a slot
 *after* that read simply aren't included in this call's results; they'll show up on the
 next `drain()`. If you need "everything that will ever be logged," you drain repeatedly,
 not once.
- **A message longer than 47 bytes is silently truncated**, not rejected or flagged, the
 fixed-size `message` field is exactly what makes zero-allocation possible, and the
 trade-off is that arbitrarily long messages simply don't fit. A caller that needs longer
 messages needs a different field layout (or a separate, allocated overflow path, which
 reintroduces the allocation cost this design exists to avoid).
- **This provides no ordering guarantee *across* producers**, only that each producer's own
 messages keep their relative `seq` order via the slot they were assigned. Two producers'
 messages can land in either relative order in the buffer depending purely on which thread's
 `fetch_add` happened to run first, if you need a genuine global happens-before order
 across producers, you need something coordinating them beyond "whoever calls first gets
 the lower slot," which by itself only reflects scheduling, not any real causal relationship
 between the two producers' work.

## 7. Interview follow-ups

**"How much does zero-allocation actually buy you here, is this a real optimization or a
premature one?"** Measured on this machine, 8 threads, 300,000 logs each: a mutex-guarded
queue with a heap-allocated `std::string` per message ran at **~204-237 ns/log**; the
fixed-slot, atomic-claim, zero-allocation design ran at **~112-115 ns/log**, roughly
**1.8-2x**. Real, but not dramatic, most of that gap is from removing the lock (every
producer serializing on one mutex), with the allocator's own cost as a second, smaller
contributor. If your logging volume is low relative to everything else your threads do, this
optimization won't be the one that matters; if you're logging on a genuinely hot path (many
calls per second, per thread), it is.

**"What happens under 10^8 log calls/sec across many producers, what's the actual
bottleneck?"** The single shared `next_write_` atomic. Every producer's `fetch_add` touches
the same cache line, so at extreme scale that counter itself becomes the serialization
point, the same shape of problem [[038-multithreaded-memory-pool]]'s single `head_` atomic
hits under heavy contention. The fix at that scale is usually sharding, give each producer
(or each small group of producers) its own sub-buffer and counter, and have the consumer
drain all the shards, trading a single global order for much less cross-core contention.

**"A producer crashes (or throws) after claiming a slot but before finishing the write,
what does the consumer see?"** With this design: nothing, forever, the slot's `ready_` flag
never gets set, so `drain()` correctly never returns it, but that slot (and its share of
capacity) is permanently lost for this buffer's lifetime, since there's no wraparound to
reclaim it. In a real system this is a genuine failure mode worth guarding against, e.g., a
producer that writes into a local `LogRecord` first and only publishes it into the shared
slot with a single final store, so a mid-write crash can't leave a claimed-but-unpublished
slot at all.
