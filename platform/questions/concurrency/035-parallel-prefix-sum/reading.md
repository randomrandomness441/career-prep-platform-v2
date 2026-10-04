## 1. Reframe the problem

A prefix sum looks like it can't be parallelized at all, `result[i]` depends on `result[i-1]`
which depends on `result[i-2]`, all the way back to the start, a chain of dependencies as
long as the array. The trick is noticing that the dependency is only *within* a chunk if you
split the work up front: a thread computing the local prefix sum of elements 1000-1999 needs
to know nothing about elements 0-999 to get the *shape* right, every value in its chunk is
some fixed local total plus one unknown number, the sum of everything before the chunk
started. Compute the local shape for every chunk in parallel (genuinely independent, no
coordination needed), and separately, cheaply, work out each chunk's missing carry-in, then
one more parallel pass applies it. The sequential dependency doesn't go away; it gets
compressed from "one link per element" down to "one link per chunk."

## 3. The broken version, first

The boilerplate does exactly the first half of that idea, and stops:

```cpp
threads.emplace_back([&, lo, hi] {
    long running = 0; // starts at 0 for EVERY chunk
    for (std::size_t i = lo; i < hi; ++i) { result[i] = running; running += input[i]; }
});
```

**Why it looks right:** each thread's own loop is a completely correct prefix sum, of its
own chunk. There's no race, no shared state touched during the parallel part at all, and if
you eyeball chunk 0's output it's exactly right. The bug isn't a concurrency bug in the usual
sense (nothing here would make TSan blink), it's a missing piece of the algorithm, and it's
invisible unless you check a chunk other than the first.

Running it on `{1, 2, 3, 4, 5}` split across 4 threads:

```
small: result[2] = 0, expected 3 (exclusive prefix sum)
```

Wrong on the very first non-trivial case, every time, this isn't a race that fires
occasionally, it's a deterministic bug that fires on every run, because it's not a timing
problem at all. Chunk 0 happened to be right by accident (there's genuinely nothing before
it to carry in); every other chunk is missing the sum of everything that came before it.

The fix adds the middle step: a short sequential pass over the chunk totals, turning them
into starting offsets, then a second parallel pass that applies them:

```cpp
for (int t = 0; t < num_threads; ++t) { chunk_offset[t] = running; running += chunk_total[t]; }
```

Same test, plus uneven chunk counts and 500,000-element random trials:

```
small/degenerate cases, uneven chunk division, and 4 large-random trials
(500000 elements, 8 threads): every prefix sum exact
```

## 6. Where this solution fails

- **The middle step is sequential and its cost scales with `num_threads`, not with `n`**,
 fine at 8 or even a few dozen threads, but a design that scales to hundreds of threads
 would want that step parallelized too (a small prefix sum over the chunk totals, applying
 this exact same technique recursively) rather than assuming it's always negligible.
- **This is a two-pass algorithm with a full synchronization point between the passes**,
 every thread must finish pass 1 before pass 2's chunk offsets can be computed, and every
 thread must see the finished offsets before pass 3 starts. There's no way to stream this;
 the whole array has to be touched twice.
- **Measured, and worth taking seriously: this is a memory-bandwidth-bound operation, and
 parallelizing it doesn't reliably help on this machine.** 20,000,000 `long`s (160 MB, well
 past any cache), 8 threads, 5 repeated trials in the same process: speedup ranged from
 **0.56x (slower than sequential) to 1.34x**, run to run, on identical input. One add and
 two memory accesses per element is too little arithmetic per byte moved for extra cores to
 help much, the bottleneck is how fast data can move between memory and the CPU, not how
 many cores are adding numbers, and more threads competing for that same memory bandwidth
 can make things *worse*, not better. Compare this to [[034-parallel-argsort]]'s
 false-sharing fix, which measured a clean, stable ~100x, the difference isn't the
 technique, it's that this workload has almost no computation to hide the memory cost
 behind, and that one did.

## 7. Interview follow-ups

**"You measured this as sometimes SLOWER in parallel, doesn't that mean the code is
wrong?"** No, it means the algorithm is correct and the workload is a poor fit for
parallelism on this hardware. Prefix sum over a huge array does exactly one arithmetic
operation per element while touching two cache lines (read `input[i]`, write `result[i]`);
once you have enough cores to saturate the machine's memory bus, adding more threads doesn't
increase throughput, it just adds scheduling and thread-creation overhead on top of the same
bandwidth ceiling. This is precisely why "measure, don't assume" (see also
[[013-store-buffering]], [[030-atomics-vs-mutexes]], [[038-multithreaded-memory-pool]] this
same course) matters even for something as textbook-parallel-friendly as a scan.

**"When would parallelizing a scan actually pay off, then?"** When there's real computation
per element to hide behind the memory access, a prefix-max of a struct requiring a
comparison function call, a running combine of larger objects, or simply enough arithmetic
intensity that compute, not memory bandwidth, is the bottleneck. The three-pass structure
itself is unchanged; whether it's worth parallelizing is a property of what's inside the
loop, not of the scan pattern itself.

**"100M elements, need this at 10^8 elements/sec, what's the actual limiting resource?"**
Memory bandwidth, almost certainly, given the measurement above, the fix isn't more threads,
it's reducing bytes moved (a narrower element type if the value range allows it, or
restructuring so results aren't fully materialized if the caller can consume them streaming)
or accepting that this operation is bandwidth-bound and budgeting for the memory system's
actual throughput rather than the core count.

**"How does this generalize past addition, could you compute a running maximum, or a
running string concatenation, with the same three-pass structure?"** Yes, for any
associative operation (the "carry" step needs `combine(chunk_totals[0..t-1])` to be
well-defined regardless of grouping), maximum works identically; string concatenation works
but loses the "cheap because chunk totals are small numbers" property, since a chunk's
"total" is now itself a potentially large string, changing the cost profile of the merge
step entirely.
