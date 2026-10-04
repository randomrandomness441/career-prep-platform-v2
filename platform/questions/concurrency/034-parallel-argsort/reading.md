## 1. Reframe the problem

Sorting a huge array with only a few hundred distinct values isn't really a sorting problem
, it's a *counting* problem. Count how many of each value exist, turn those counts into
"where does each value's group start in the output," then place every element into its
group. No comparisons, no `O(n log n)`; the whole thing is linear in `n` plus a pass over the
small number of buckets. That's counting sort, and it's the right tool specifically because
`num_buckets` is small relative to `n`, the moment you're tempted to reach for
`std::sort`, you've thrown away the one fact (few distinct values) that makes this problem
easy.

Parallelizing it, the instinct is to parallelize every step the same way: give each thread a
slice of the array, have it touch shared counters directly. That instinct is right for the
*scatter* step (each element's final position is independent, claimed with one atomic op)
and wrong for the *counting* step, and the difference between those two steps is this
question's whole lesson.

## 3. The broken version, first

The boilerplate's counting pass has every thread increment one shared array directly:

```cpp
std::vector<std::size_t> count(num_buckets, 0);
// ... inside each thread's loop:
++count[values[i]];
```

**Why it looks right:** it's the most direct translation of "count how many of each value",
one array, one entry per bucket, increment the right one for each element you see. Nothing
about the code signals a problem; `count[values[i]]` reads as an ordinary array update, the
same shape you'd write in a sequential version, just wrapped in a thread.

Running it, 200,000 elements, 500 buckets, 8 threads, several trials:

```
large-random: index 0 is out of range or appears more than once in the result --
not a valid permutation
```

Lost counts in the histogram meant some buckets' final counts were smaller than the true
number of elements in them, which meant the offsets computed from those counts were wrong,
which meant the scatter pass (itself correct in isolation) placed elements using bad
starting positions, producing a `perm` that isn't even a valid permutation of the input
indices anymore. One race early in the pipeline corrupted everything downstream of it.

The fix, a local histogram per thread, merged sequentially after counting finishes:

```cpp
std::vector<std::vector<std::size_t>> local_counts(num_threads, std::vector<std::size_t>(num_buckets, 0));
// each thread only ever touches local_counts[its own index]
// after joining: merged[b] = sum over threads of local_counts[t][b]
```

Same 200,000-element, 500-bucket run, four trials:

```
small, degenerate, and 4 large-random trials (200000 elements, 500 buckets, 8 threads):
every result a valid, correctly sorted permutation
```

## 6. Where this solution fails

- **The merge step is sequential, and it's `O(num_threads x num_buckets)`, that stops being
 "cheap" if buckets ever gets large.** With 500 buckets and 8 threads this is 4,000
 additions, negligible next to counting 200,000 elements. With, say, 10 million distinct
 values, the merge itself becomes a serial bottleneck the counting phase's parallelism
 doesn't help with, this design's "few distinct values" assumption is load-bearing, not
 incidental.
- **The scatter phase's per-bucket atomic cursors still serialize elements that share a
 bucket, even across threads that have nothing else to do with each other.** For a highly
 skewed distribution (most elements landing in one or two buckets), that single bucket's
 atomic becomes exactly the contended-hot-cache-line problem this question's local-histogram
 fix avoided for counting, the scatter phase doesn't get the same fix here, because unlike
 counting, the scatter's final *positions* genuinely have to be assigned by some shared
 arbiter.
- **This doesn't generalize to sorting by a key with unbounded range.** The entire technique
 depends on `num_buckets` being small and known ahead of time; it says nothing about sorting
 arbitrary comparable values, which is a fundamentally different (and harder-to-parallelize
 well) problem.

## 7. Interview follow-ups

**"How much does the local-histogram fix actually save, versus just making the shared
histogram atomic?"** Measured here: 20,000,000 elements, 500 buckets, 8 threads, local
histograms plus a sequential merge: **~2.3-2.7 ms**. The same counting done with one shared
`std::atomic<std::size_t>` per bucket, `fetch_add` from every thread: **~270 ms**, roughly
**100x slower**. With 500 buckets shared across 8 threads doing effectively random accesses,
most `fetch_add` calls land on a cache line another core just touched, turning every count
into a cache-coherence round trip; a purely local array has no cross-core traffic at all
until the (tiny) merge step.

**"Why not just use atomics for the scatter phase too, to keep the code uniform?"** It
already does, `fetch_add` on a per-bucket cursor is exactly the right tool there, because
each element's *final position* genuinely has to be assigned by some single source of truth
shared across threads (two elements of the same value can't both claim the same output
slot). The difference from counting: during counting, nothing needs to be *shared* until
everyone's done, each thread's count is a self-contained fact about its own slice. During
scattering, the assignment is inherently a shared resource. Reaching for the same tool
(atomics) everywhere ignores that these two steps have genuinely different structure.

**"100M elements, <1000 distinct values, 10^8 ops/sec target, what's the actual
bottleneck at that scale?"** Memory bandwidth, not compute or synchronization, once the
counting-phase fix above is in place, reading 100M `int`s and writing 100M `std::size_t`s
in the scatter phase is a bandwidth-bound streaming pass, and 8+ cores will saturate a
typical memory bus well before contention on 1000 atomic cursors becomes the limiter
(1000 cursors spread the scatter-phase contention thin enough that it's a secondary cost,
not the primary one, unlike the counting phase's 500-bucket shared-histogram disaster above).

**"A downstream consumer needs the sort to be stable (equal elements keep their original
relative order), does this design support that with a small change?"** Not with the current
scatter mechanism (a plain `fetch_add` per bucket assigns positions in whatever order threads
happen to race to them, not original index order). A stable version needs each thread's
local count *and* its per-bucket starting offset relative to the other threads' local counts
for that same bucket, essentially, a full parallel prefix sum across the
`num_threads x num_buckets` local-count table, not just a merge into one global histogram.
See [[035-parallel-prefix-sum]] for that exact structure.
