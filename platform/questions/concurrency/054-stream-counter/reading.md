## 1. Reframe the problem

A counter that's incremented from many threads is the smallest possible concurrency
problem, and it's tempting to reach straight for `std::atomic<long>` and stop thinking. That
is correct, but it isn't the *fast* answer once increments are frequent enough and threads
numerous enough, because every thread's `fetch_add` is still fighting every other thread's
for the same cache line. The reframe: a counter used far more for writing than for reading
doesn't need one shared number kept exactly up to date at all times, it needs many
independent numbers, each cheap to update, combined only on the rare occasions anyone
actually reads the total.

## 3. The broken version, first

The boilerplate is a plain `long`, incremented directly:

```cpp
void increment() { ++count_; }
```

**Why it looks right:** it's the simplest possible thing that could work, and single-
threaded, it's exactly correct. The bug only exists because many threads call this at once.

Running it, 16 threads, 200,000 increments each:

```
total() = 625094, expected 3200000 -- 2574906 increments were lost under
concurrent load
```

Over 80% of increments vanished. `++count_` on a plain `long` is a read-modify-write with no
exclusion; two threads reading the same old value before either writes back is common, not
rare, under sustained contention.

The fix shards the counting across `num_shards` independent atomics, one per (roughly) each
thread:

```cpp
std::size_t shard = std::hash<std::thread::id>{}(std::this_thread::get_id()) % shards_.size();
shards_[shard].fetch_add(1, std::memory_order_relaxed);
```

Same test: exact total, every run.

## 6. Where this solution fails

- **Two threads can still hash to the same shard**, `std::hash<std::thread::id>` gives no
 guarantee of spreading threads evenly across `num_shards` buckets, so with few shards or
 bad luck, some contention can remain. More shards reduces the chance, at the cost of more
 memory and a slower `total()`.
- **Measured, honestly: the win here is real but modest, not dramatic.** 8 threads, 2,000,000
 increments each: sharded counter **16-25 ns/increment**, single shared atomic
 **27-56 ns/increment**, noticeably faster and much more *stable* run to run (the single
 atomic's contention makes its own timing noisy), but not an order of magnitude. Compare to
 [[029-false-sharing]]'s ~36x for padding independent counters, the difference is that a
 single contended atomic here is still one cheap instruction per increment on this
 hardware; sharding removes the contention, but contention on a single relaxed atomic was
 never as catastrophically expensive as unpadded false sharing was in that question.
- **`total()` is only ever an approximation of "the count at this exact instant"**, by the
 time the last shard is summed, an increment could have landed on the first shard already
 summed. For this exercise's contract (call `total()` after all increments have finished)
 that's fine; a caller wanting a live, always-consistent running total during active
 increments needs a fundamentally different guarantee than a sharded counter provides.
- **Shard count is fixed at construction**, a workload whose thread count changes
 dramatically over the counter's lifetime (many short-lived threads, or a thread pool that
 resizes) doesn't get to adapt `num_shards` to match.

## 7. Interview follow-ups

**"Why would you ever pick the plain single-atomic version over sharding, given sharding
measured faster and more stable?"** Simplicity, and `total()`'s cost: a single atomic's read
is one load; a sharded counter's read is `num_shards` loads, each potentially a cache miss.
For a counter read far more often than it's written (the opposite of this exercise's stated
workload), a single atomic is both simpler and cheaper on the read side, sharding is a
write-optimization, and it costs something on the read path in exchange.

**"10^8 increments/sec target, many more threads than shards, what happens?"** Multiple
threads sharing a shard via `hash() % num_shards` start contending on that shard exactly like
the single-atomic version did, just among a subset of threads instead of all of them. The
fix at that scale is either more shards (more read-side cost) or assigning shards more
carefully (e.g., a thread-local shard index chosen once, rather than rehashing the same
`thread::id` on every call, which at least removes the hash computation from the hot path
even though it doesn't change the underlying collision math).

**"How would you extend this to more than a running total, say, a histogram of values, not
just a count?"** Same sharding idea, applied per-bucket instead of to one number, this is
almost exactly [[034-parallel-argsort]]'s local-histogram-then-merge technique, just
continuously live (each thread's local buckets keep accumulating) instead of a one-shot
counting pass over a fixed array.
