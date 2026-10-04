## 1. Reframe the problem

Two threads, two counters, no shared logic, no lock, no atomic contention in the sense
you've seen so far, thread A only ever touches `counters[0]`, thread B only ever touches
`counters[1]`. By every rule you've learned in this course, that's uncontended. There is no
race, nothing to synchronise, nothing to reason about.

And yet on real hardware it can run **thirty times slower** than the same two threads
writing to two counters that live far apart in memory. The bug isn't in your code at all,
it's in a fact about the hardware that your code never mentions: **the cache doesn't move
data at the granularity your variables are declared at.** It moves fixed 64-byte blocks
called cache lines. If two "unrelated" variables happen to fall in the same block, the
hardware treats them as one variable for the purposes of cache coherence, whether your
program treats them that way or not.

This is **false sharing**: the two threads *look* independent and *are* independent in
every way the language gives you words for, but the cache-coherence protocol shares them
anyway.

## 2. The tools

### The layout that causes it

```cpp
std::atomic<long> slots_[8]; // 8 bytes each, 64 bytes total -- one cache line
```

Eight `long`s, eight bytes apiece, sixty-four bytes total. That's exactly one cache line on
every mainstream CPU (x86, ARM, Apple Silicon). Every one of those eight counters lives on
the *same* line.

### What a cache line actually is

Picture the cache not as a bag of individual variables but as a shelf of 64-byte boxes.
A core can't take just one byte out of main memory, it always pulls a whole box onto its
shelf, and it can't put a byte back either; it writes the whole box back. Two threads on two
different cores each keep their own copy of a box on their own shelf. Cache-coherence
hardware (MESI or a variant of it) keeps those copies truthful: the instant one core writes
to *any byte* in its copy of a box, every other core's copy of that same box is marked
invalid, not just the byte that changed, the *entire box*. The next time the other core
touches anything in that box, even a byte it never wrote to, it has to fetch the box again.

### The fix: pad each counter to its own line

```cpp
struct alignas(64) Slot { std::atomic<long> v{0}; };
Slot slots_[8];
```

`alignas(64)` tells the compiler two things at once: every `Slot` object must *start* on a
64-byte boundary, and, because array elements must be contiguous, `sizeof(Slot)` must
itself be padded up to a multiple of 64. A struct holding one 8-byte atomic becomes 64 bytes
of storage, 56 of them unused. That waste is the entire fix: it guarantees no two `Slot`s
can ever land in the same cache line, no matter what the allocator or the stack does.

### How to check you actually did it

Don't eyeball the struct, check the addresses:

```cpp
std::uintptr_t a = reinterpret_cast<std::uintptr_t>(&slots_[0]);
std::uintptr_t b = reinterpret_cast<std::uintptr_t>(&slots_[1]);
assert(a / 64 != b / 64); // different 64-byte-aligned block
```

That one-liner is the entire test this exercise runs. It's deterministic, it depends on
where the compiler placed the objects, not on how the threads happen to get scheduled, so
it fails every single run against the unpadded layout and passes every single run against
the padded one. No timing measurement needed to prove the bug exists.

## 3. The broken version, first

```cpp
template <std::size_t N>
class CounterBank {
    std::atomic<long> slots_[N]; // <-- the bug
    // ...
};
```

**Why it seems correct, and who it fools:** it *is* correct. `get(i)` after `T` calls to
`increment(i)` always returns exactly `T`. There's no data race by the language's own
definition, each index is written by exactly one thread, same as the previous SPSC ring
buffer exercise. Every unit test that checks answers, not addresses, goes green. A reviewer
reading the diff sees a plain array of atomics, one index per thread, no aliasing, the
textbook picture of "obviously fine." Nothing about the *source code* tells you eight of
these live on top of each other in memory; that's a fact about `sizeof(long)` and cache
geometry that the type system has no way to surface.

Run it for real, 8 threads, each hammering its own counter with a relaxed
`fetch_add`, 2,000,000 increments per thread, minimum of 9 repetitions, this machine
(Apple Silicon, 10 cores):

```
unpadded (all 8 counters share one cache line): 441.6 ms (median)
padded (alignas(64), one counter per line): 12.1 ms (median)
ratio: 36.5x
```

Same instructions, same total work, same absence of any data race. The padded version
does roughly **6 ns per increment**; the unpadded version does roughly **220 ns per
increment**, two orders of magnitude apart, entirely explained by eight cores fighting
over one 64-byte line on every single write.

**Why the fix is what production code does.** `alignas(64)` on each element is not a hack
that happens to work here, it's the standard, minimal statement of the actual invariant:
*"nothing else may live in this cache line."* It costs exactly what it says it costs (56
wasted bytes per counter here) and buys exactly what it promises. That directness, pay for
the property you need, name it in the type, verify it with a pointer subtraction, is what
a senior reviewer is looking for over "I moved the array around until it got faster."

## 4. Real-world usage

**Why this matters in production.** Multi-core hardware got cheap decades before most
programmers' mental model of "does this variable need a lock" caught up to it. False
sharing is the tax you pay for thinking about correctness (does two threads touching
different memory need synchronisation, no) without thinking about mechanical sympathy
(does the *hardware* treat that memory as separate, not always). It was invented as a
named, diagnosable problem once multi-socket and multi-core SMP systems became common in
the 1990s and people started finding "correct" code that didn't scale.

**Where you meet it:**

- **Per-thread counters and statistics.** Exactly this exercise, a metrics array, one
 slot per worker thread, incremented on a hot path. This is the single most common place
 false sharing shows up in real systems, because it's the most natural way to write a
 sharded counter.
- **Producer/consumer indices**, as in the SPSC ring buffer question, `head_` and `tail_`
 sitting next to each other is the same bug wearing a different hat, and that exercise pads
 both indices for exactly this reason.
- **Lock striping / sharded data structures.** An array of per-shard mutexes or per-shard
 free-lists has the same failure mode if the shard headers are packed tightly.
- **Thread-pool / work-stealing queues.** Each worker's local deque head sitting near
 another worker's in a flat array is a classic false-sharing report in profilers.

**Where NOT to reach for padding:**

- **Read-mostly, rarely-written data.** False sharing is a *write* problem, it's the
 invalidation on write that costs you. Data that's written once and read by everyone
 afterward doesn't benefit from padding; you're just spending memory.
- **Low thread counts or low contention.** With one thread, or with writes rare enough that
 two cores are almost never touching the same line at the same instant, the effect is
 negligible and 56 bytes of padding per field is pure waste.
- **Before you've measured.** This is a micro-optimisation with a real memory cost (this
 exercise's 8-`long` bank goes from 64 bytes to 512). Reach for it when a profiler shows
 cache-line contention (high L2/L3 invalidation rates on a hot structure), not
 speculatively on every shared array you write.
- **As a substitute for reducing contention in the first place.** If ten threads are
 fighting over one shared counter, padding doesn't remove the fight, it removes only the
 *false* part of the sharing. A genuinely shared, frequently-written atomic is still
 expensive no matter how it's aligned; the real fix there is usually per-thread counters
 summed occasionally, not padding.

## 5. Performance

Measured on this machine (Apple Silicon, 10 cores, 8 threads, `-O1` to match this
platform's correctness build and `-O2` for the headline numbers below, both showed the
same effect), relaxed `fetch_add`, 2,000,000 increments per thread, min/median/max of 9
repetitions:

| layout | ns / increment (per thread) | 16M increments, wall time |
|---|---|---|
| unpadded, 8 counters share 1 line | **~220 ns** (range ~204–272 ns) | **441.6 ms** median |
| padded, `alignas(64)` per counter | **~6.0 ns** (range ~5.9–7.5 ns) | **12.1 ms** median |

**Ratio: ~36x**, and it was not a fluke, every one of 9 repetitions in the padded case
landed within a millisecond of the others, and every unpadded repetition was slower by at
least 27x, never less. This is a much larger and much more *stable* effect than the
`alignas(64)` measurement attempted for the SPSC ring buffer question, where two threads
gave a swing too noisy to report a number. The difference is thread count: with 8 cores all
hammering the same line at once instead of 2, the false-sharing cost dominates so heavily
that scheduling noise (P-core vs E-core placement) can't hide it any more.

**A smaller, sharper number:** an uncontended relaxed atomic increment on this machine costs
roughly 6 ns start to finish. The false-shared version costs 220 ns, the other **214 ns is
pure cache-coherence traffic**, cache-line ping-pong between eight cores' L2/L3 slices, for
work that touches no memory the other threads care about at all.

## 6. Where this solution fails

- **Padding costs real memory, and it compounds.** 8 counters go from 64 bytes to 512.
 A per-shard structure with 256 shards and three padded fields each is measured in
 megabytes before it holds a single byte of actual data. Pad only fields that are actually
 hot and actually shared-but-shouldn't-be.
- **It only helps writers on different cores.** If all your threads share one core (a
 single-core VM, a container with a tight CPU limit), there's no cache-coherence traffic to
 eliminate, the "fix" only spends memory.
- **`alignas` on a struct doesn't protect a single scalar sitting on the stack next to
 something else.** Padding fixes *this* array's layout; it says nothing about a `long`
 that happens to be adjacent to a mutex or another hot atomic elsewhere in the same object,
 which is a separate instance of the same bug and needs its own `alignas`.
- **Two padded fields *inside the same struct* can still false-share** if you pad one and
 forget the other, say a padded counter next to an unpadded flag the same thread also
 writes often. The check in this exercise only verifies the counters against each other;
 it says nothing about anything else you add to the class later.
- **Reads don't need this.** A structure that's written once at startup and only read after
 that gains nothing from padding, the invalidation storm this fixes only happens on
 concurrent *writes* to the same line. Padding a read-mostly table is diagnosed the same
 way it's created: by measuring, not by pattern-matching "shared array, better pad it."
 Get this wrong and you've spent memory on nothing.
- **It doesn't fix true sharing.** If the counters in this exercise were all incremented by
 *every* thread instead of one owner each, padding buys nothing, the contention is real,
 not false, and the fix is a different data structure (per-thread partial counters summed
 on read), not better alignment.
- **The magic number 64 isn't universal.** Some ARM and older x86 parts use different line
 sizes, and Intel's L2 prefetcher can behave as if the effective line is 128 bytes
 (adjacent-line prefetch). Code that must be portable across unknown hardware sometimes
 pads to 128 defensively; `std::hardware_destructive_interference_size` (C++17,
 `<new>`) is the standard's attempt to give you the right number for the target platform
 instead of hand-picking 64.

## 7. Interview follow-ups

**Q: Why is it slow?**
A: `arr[0]` and `arr[1]` live on the same CPU cache line. When one core writes, the
cache-coherence protocol invalidates every other core's copy of that *entire* line, not
just the byte that changed, so the next access from another core is a full cache miss,
even for a byte it never touched.

**Q: How do you fix it?**
A: Pad each hot, independently-written field onto its own cache line, `alignas(64)` on a
wrapper struct, or an explicit padding array. Verify it, don't assume it: check that the
addresses land in different 64-byte-aligned blocks.

**Q: How do you detect false sharing in a real system, without a hunch?**
A: `perf c2c` on Linux was built specifically for this, it points at the exact cache line
and the exact instructions on each core fighting over it. Lacking that, `perf stat -e
cache-misses,cache-references` showing a miss rate wildly out of proportion to memory
volume is the smell; a profiler showing time inside atomic RMW instructions that shouldn't
be contended (per-thread, unshared data) is the other tell.

**Q: Small machine, 1–2 cores, one parked for the OS. What changes?**
A: The effect can vanish almost entirely. False sharing is fundamentally about *two
different cores'* caches disagreeing; with effectively one core running your threads, the
scheduler is time-slicing, not truly parallelizing, and there's no second core's cache line
to invalidate. Don't pad speculatively for a target you haven't profiled on.

**Q: Big machine, 64+ cores, NUMA. What gets worse?**
A: The 36x this exercise measured is with 8 cores on one socket sharing on-die L2/L3. Across
NUMA nodes the "fetch the line from another core" trip becomes "fetch the line from another
socket's memory controller over the interconnect", an order of magnitude further, and now
also consuming interconnect bandwidth that other unrelated traffic on the machine needs.
False sharing that was an annoyance on one socket can become the dominant cost on a NUMA
box.

**Q: Extreme load, 10^8 ops/sec across many threads on padded, unshared counters. What
saturates first?**
A: Not the cache line, you've removed that contention by construction. At that rate you're
now bound by raw memory bandwidth (writing that much data back to caches/memory) or, more
likely, you've moved the bottleneck to whatever *reads* these counters, a monitoring
thread summing 256 padded slots is now walking 16 KB instead of 2 KB of cache-cold memory
per read, purely because of the padding you added.

**Q: A field gets added to `Slot` later, say a timestamp next to the counter. Does the fix
still hold?**
A: Only if the new field is meant to be touched by the same owning thread. If a *different*
thread reads or writes that timestamp, you've reintroduced sharing inside the padded slot,
padding protects the slot from its *neighbors*, not from something new added inside it.
Re-check the invariant, don't assume `alignas(64)` on the struct grandfathers in whatever
you add next.

**Q: How would you catch a regression here in CI, not just in a one-off benchmark?**
A: A benchmark's wall-clock number is too noisy to gate a build on (see section 5's
`alignas` discussion for the SPSC exercise). What *is* deterministic and gate-able is
exactly what this question's test does: assert on the addresses. `static_assert(alignof(Slot)
>= 64)` plus a runtime pointer-difference check catches "someone removed the `alignas`" on
every single run, with zero flakiness, that's a far better regression test than a timing
threshold.
