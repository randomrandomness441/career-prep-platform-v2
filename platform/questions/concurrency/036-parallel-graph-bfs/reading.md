## 1. Reframe the problem

Sequential BFS never has to think about two things that parallel BFS can't avoid. First:
what happens when two different current-level nodes both have an edge to the *same*
next-level node? Sequentially, whichever one is processed first marks it visited and the
second one's check simply sees "already visited" and moves on, there's no way for both to
"win" at once, because there's only one thread doing the checking. Second: many workers are
all discovering new nodes for the next level at the same time, and somewhere those
discoveries have to land in one shared structure, but that structure doesn't need to be
touched *while* the discovering is still happening, only once each thread already knows what
it found.

The mental shift: `visited[v]` isn't a plain flag you read then write, it's a resource
exactly one thread is allowed to "win," which is a `compare_exchange` problem, not an `if`
problem, and building the next frontier is a merge you can defer to the end of the level,
not something that has to happen live while threads are racing each other to find nodes.

## 3. The broken version, first

The boilerplate's discovery check is the ordinary-looking version:

```cpp
if (!visited[v]) {
    visited[v] = true;
    dist[v] = level + 1;
    std::lock_guard<std::mutex> lk(push_mutex);
    next_frontier.push_back(v);
}
```

**Why it looks right:** there *is* a lock here, `push_mutex` protects the shared vector,
which is the part of this code that most obviously involves multiple threads touching one
piece of memory. It's easy to look at a mutex guarding the append and conclude the whole
block is safe, without noticing that the check-and-set on `visited[v]`, one line above the
lock, isn't inside it at all.

This is a different kind of failure from most of this course's boilerplates: run it against
the correctness tests alone, and it **passes**:

```
solution.cpp -> CLEAN
boilerplate.cpp -> CLEAN (correctness stage only)
```

On the graphs this question tests, two threads racing to claim the same node happens to
compute the same `dist` value either way (both write `level + 1`), so the *distances* come
out right. Only under ThreadSanitizer does the actual defect show up:

```
WARNING: ThreadSanitizer: data race
 Read of size 8 by thread T3 ... previous write of size 8 by thread T2
```

, a genuine, unguarded concurrent read/write on `visited`, exactly the kind of "right answer
today, undefined behaviour by the standard" result this whole platform's three-tier verdict
(`CLEAN` / `CORRECT_BUT_RACY` / `REJECTED`) exists to catch instead of missing. This is what
`CORRECT_BUT_RACY` means in practice: the boilerplate here doesn't fail a single test, and
it's still wrong.

The fix replaces the check-then-act with one atomic operation, and moves the frontier merge
out of the parallel section entirely:

```cpp
bool expected = false;
if (claimed[v].compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
    dist[v] = level + 1;
    local_next[t].push_back(v); // this thread's own vector, not shared
}
```

Full sweep on the fix: `CLEAN`, 12/12 correctness, 20 clean TSan runs, 150 clean shaken
runs.

## 6. Where this solution fails

- **The correctness test alone would never have caught this**, which is the point worth
 sitting with: a check-then-act race on a boolean is not guaranteed to produce a visibly
 wrong answer on any particular run, or even on most runs. Relying on "the tests pass" as
 your only signal for concurrent code is exactly the gap TSan (and this platform's
 three-tier verdict) exists to close.
- **`compare_exchange_strong` with `memory_order_relaxed` is enough here specifically
 because nothing about *ordering* matters, only atomicity of the claim itself**, `dist[v]`
 is written only by the one thread that won the exchange, so there's no cross-thread
 visibility requirement beyond "exactly one winner." A design that needed the *order* of
 discoveries to matter (not the case for plain BFS distance) would need to think harder
 about memory order here.
- **The per-level synchronization (every thread finishes the current level before the next
 one starts) is a real serialization point.** For a graph with a very uneven frontier size
 from level to level (a huge fan-out at one level, almost none at the next), the pool is
 underutilized on the small levels, the parallelism only helps as much as the widest
 frontier allows.
- **This assumes the whole graph fits in memory as an adjacency list already built**, it
 says nothing about crawling a graph that's discovered incrementally (fetching neighbours
 is itself slow, or unbounded), which is [[031-web-crawler-multithreaded]]'s problem, not
 this one's.

## 7. Interview follow-ups

**"The boilerplate passed every correctness test, how would you have caught this bug before
it shipped, without already knowing to look for it?"** Exactly the way this question's own
authoring did: run the correctness suite under ThreadSanitizer, not just as a plain build.
A check-then-act race on ordinary memory (not an atomic) is precisely the missing-
synchronization pattern TSan is built to catch, and it caught this one on the first
instrumented run despite the plain build never producing a wrong answer in testing.

**"Why does compare_exchange fix this and a mutex around the whole block wouldn't have been
just as good?"** A mutex around the check-and-set would also fix it, the actual bug is that
the check and the set weren't atomic together, and either a lock or a compare-exchange closes
that gap. `compare_exchange` was chosen here because the claim itself is the only thing that
needs exclusivity (one bit per node, one atomic op), and avoiding a mutex means avoiding
mutex overhead on a hot per-edge check that happens millions of times per level, for a
single boolean claim, atomics genuinely are the lighter tool.

**"Big machine, huge graph, very wide frontiers, what breaks first?"** The sequential merge
step (concatenating each thread's local next-frontier vector into one) becomes a real cost
once frontiers are large enough, it's `O(frontier size)` work done by one thread while every
other thread waits. At extreme scale, real parallel BFS implementations parallelize the
merge too (each thread's local vector's final position in the merged array can itself be
computed via a parallel prefix sum over the local vector sizes, see
[[035-parallel-prefix-sum]] for exactly that structure) rather than accepting a sequential
merge as "cheap enough."

**"How would you adapt this to a weighted graph (shortest path by total edge weight, not hop
count)?"** Level-synchronous BFS only works because every edge has the same "cost" (one
hop), the whole reason you can process an entire frontier in parallel is that everyone at
distance `k` is guaranteed to have been discovered before anyone at distance `k+1`. Weighted
shortest paths (Dijkstra) don't have that clean level structure; parallelizing them needs a
different approach (Δ-stepping is the classic one), not a straightforward adaptation of this
design.
