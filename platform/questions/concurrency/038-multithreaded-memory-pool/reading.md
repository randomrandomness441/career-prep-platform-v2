## 1. Reframe the problem

The obvious way to make an allocator thread-safe is a mutex around every call, and that's
actually fine for the free list itself; a `lock_guard` around a few pointer swaps is cheap
and correct. The trap in this question isn't the free list. It's the one piece of state that
looks like it doesn't need the same care: `blocks_in_use()`, a running counter that's easy to
reason about as "just a stat" rather than as shared, mutated state with exactly the same
concurrency requirements as everything else in the class.

There's a second, deeper trap this question's authoring ran into directly, worth reframing up
front: a memory pool's free list isn't a one-shot data structure like a typical stack demo,
its nodes are *constantly recycled*. The same block gets freed and handed back out, at the
same address, seconds (or microseconds) later, and the caller immediately starts writing
real payload into it. That's exactly the setup [[015-treiber-stack]]'s ABA problem needs,
but as section 6 shows with real evidence, ABA-safety (a generation-tagged CAS) turns out not
to be the *only* hazard that recycling creates.

## 3. The broken version, first

The boilerplate's free list is correct, mutex-guarded, and every test for capacity,
exhaustion, and double-free rejection passes. The bug is one line, outside the lock:

```cpp
void* allocate() {
    std::uint32_t idx;
    { std::lock_guard<std::mutex> lk(mutex_); /* ... free-list surgery ... */ }
    ++in_use_count_; // <- outside the lock
    return block_ptr(idx);
}
```

**Why it looks right:** the reasoning that put it here is genuinely sensible on its own,
"the free-list surgery is the part that actually needs exclusion; this is just a stats
counter for observability, an `int++` is basically free, why serialize every thread on the
big lock just for that?" Every clause of that is a fair thing to think, and for a
single-threaded caller, or a lightly-contended one, it will often look fine.

Running it, 8 threads, 6,000 balanced allocate/deallocate cycles each:

```
blocks_in_use() = 75 after 8 threads x 6000 balanced alloc/dealloc cycles, expected 0
-- the in-use counter lost updates under contention
```

75 lost updates out of 96,000 total increments/decrements, small as a fraction, and exactly
the kind of drift that would be invisible in a lightly-loaded manual test and show up as a
slow, confusing metrics discrepancy in production (a pool that reports itself as "still
having 75 blocks allocated" long after every caller has freed everything it borrowed).

The fix, the shipped solution, is making the counter atomic: `std::atomic<std::size_t>`,
`fetch_add`/`fetch_sub`. That's the entire diff from the boilerplate, and it's enough to pass
every test here, including the full TSan and shaken-stress sweep.

### A third attempt this question tried, and rejected

Before settling on the mutex version above, this question's own authoring tried going
further: dropping the mutex entirely and making the free list lock-free, the same
intrusive-linked-list-via-CAS technique as [[015-treiber-stack]], with a generation-tagged
head to close the ABA hole. It compiled clean, passed all 12 correctness runs, every time.
Then TSan ran on it:

```
WARNING: ThreadSanitizer: data race (pid=16918)
 Write of size 4 ... by thread T2
 Previous read of size 4 ... by thread T1
 Location is stack of main thread.
```

A genuine, real data race, not a flaky one, reproduced on the very first TSan run. The
generation tag does close the classic ABA hole (a stale CAS can no longer *silently succeed*
just because an old index reappeared at the top of the list), but `allocate()` still has to
*read* a block's stored "next free" link before it knows whether its CAS will win, and that
read is a plain, non-atomic `memcpy`. If another thread already won the race for that same
block a moment earlier and is now writing real payload into it, that thread's payload write
and this thread's speculative "next-link" read touch the same bytes with no synchronization
between them, the first thread's CAS will simply fail and it'll retry with fresh data, so
the *result* was never wrong in this run, but the race happened regardless, and the standard
calls that undefined behavior whether or not you got unlucky this time.

This is not a bug that a smarter CAS retry loop fixes, it's the reason real lock-free
allocators use **hazard pointers** or epoch-based reclamation: a way to mark "I am currently
looking at this node" that a would-be reuser has to respect before recycling it. That's real,
substantial machinery this question doesn't ask you to build. The mutex is the honest answer
here, not a fallback, see section 6 for what it actually costs you against the lock-free
attempt, which turned out to be worse on both axes that matter.

## 6. Where this solution fails

- **`deallocate()` on a pointer this pool never handed out, that happens to alias into the
 storage buffer by chance, is only caught if it lands exactly on a block boundary and its
 in-use flag happens to say "true."** A garbage pointer that isn't from this pool at all is
 rejected reliably (it fails the bounds/alignment check); a pointer that's *almost* right
 (off by a few bytes, into the middle of some other live block) can slip past the alignment
 check if it happens to land on a block boundary anyway, this API can't distinguish "a
 pointer I own" from "a pointer that merely looks like one I could have handed out."
- **Fail-fast exhaustion pushes the "what do I do when the pool is empty" decision entirely
 onto the caller.** That's the right default for a pool meant to avoid ever blocking a
 latency-sensitive path, but it means a caller that doesn't check for `nullptr` and just
 dereferences the result has turned pool exhaustion into a crash, this API gives you no
 second option (grow, wait, fall back to the heap) if fail-fast isn't actually what a given
 use case wants.
- **The mutex serializes every allocate/deallocate through one lock, full stop**, under
 extreme contention with many cores, that's a real ceiling, and it's the ceiling a
 lock-free design is meant to remove. Section 3's attempt shows that removing it isn't free:
 a correct lock-free version here needs hazard-pointer-style protection this question
 doesn't build, and until that's added, "lock-free" and "correct" are not both true at once
 for this design.
- **Measured, for the record: even setting correctness aside, the racy lock-free attempt was
 also *slower*.** 8 threads, 500,000 tight alloc/dealloc cycles each, no work done with the
 block in between: the mutex version (the shipped solution) ran at **~50 ns/cycle**; the
 lock-free attempt ran at **~700-730 ns/cycle, roughly 14x slower**, regardless of whether
 the pool held 64 or 4096 blocks. Every call in the lock-free design touches the *same*
 `head_` cache line, on every thread, via a spinning CAS retry loop with no backoff, under
 sustained contention that's a cache-line ping-pong storm across every core, worse than a
 mutex's contended path, where the OS parks waiting threads instead of spinning them. The
 lesson compounds with section 3's: here, the "boring," obviously-correct mutex answer was
 simultaneously the safe one *and* the fast one. That won't always be true, but it's a real
 reminder to measure and verify before assuming a lock-free rewrite buys you anything.

## 7. Interview follow-ups

**"Walk me through the actual race in the lock-free attempt."** `allocate()`'s CAS loop reads
a candidate block's stored "next free" index via `memcpy` *before* attempting the CAS that
would claim it, it has to, that's how a compare-and-swap retry loop works, you need a
candidate new value before you can attempt the swap. The generation tag stops a *stale* CAS
from silently succeeding. But the read itself already happened by the time the CAS is
attempted, and if another thread concurrently won that same block and started writing
payload into it, that read and this write touch the same memory with no ordering between
them, a genuine race, independent of whether the CAS that follows succeeds or fails.

**"Why didn't the generation tag fix this, isn't that the whole point of tagging?"** The tag
fixes ABA specifically: "does this CAS's expected value still correctly describe reality."
It says nothing about a *read that happens before the CAS is even attempted*, by the time
you're reading the candidate node's link, you don't yet know if you'll win it, so the read
itself is unprotected. Fixing that requires knowing, before you read, that nobody else can
be concurrently repurposing that specific node, which is exactly what hazard pointers
provide and a bare tagged-CAS free list does not.

**"When would you actually justify building the hazard-pointer version instead of the
mutex?"** When the measured mutex contention is a demonstrated bottleneck for your actual
workload, many threads, genuinely brief critical sections, high call frequency, and you're
prepared to build (or bring in) the reclamation machinery correctly. Given this question's
own measurement (the naive lock-free attempt was both racy *and* 14x slower), "lock-free" is
not a shortcut to reach for casually; it's a substantial engineering investment that only
pays off once you've confirmed, by measurement, that the mutex is really the bottleneck.

**"How would you catch a subtle, low-frequency race like this one before it ships, given it
passed 12/12 correctness runs?"** Exactly how this question's own authoring caught it: run
the correctness suite under ThreadSanitizer, not just as a plain build. This race never
produced a wrong answer in this session's testing, TSan flagged it on the very first
instrumented run despite that, because it tracks the *absence of a happens-before
relationship*, not whether a given execution happened to compute the right result. A
race that "never" causes visible corruption in casual testing is not evidence of safety;
it's evidence you haven't hit the unlucky interleaving yet.
