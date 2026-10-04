## 1. Reframe the problem

A lock-free hash map sounds like it should be "a hash map, but every bucket uses a
lock-free list instead of a mutex", and for the *publish a new node* half of insertion,
that's exactly right, and [[015-treiber-stack]] already covers the technique. What's new
here is that insertion isn't just "add a node", it's "add a node *only if this key isn't
already there*," and that "only if" is a second condition riding along with the publish
step. Get the two conflated in your head and it's easy to check the condition once, publish
separately, and miss that another thread's publish could land in between those two moments,
which is exactly what this question's boilerplate does.

The second reframe: "no delete" is not a cop-out, it's what makes the rest of this question
tractable at all. A node that's been prepended to a bucket's list might be mid-traversal in
another thread's `find()` at the exact moment you'd want to free it, freeing it anyway is a
use-after-free, and the real fix (hazard pointers, or epoch-based reclamation: a way to know
no other thread could still be looking at a node before it's safe to free) is genuinely
substantial machinery. Scoping this question to insert-and-lookup-only sidesteps the hardest
part of lock-free data structures entirely, on purpose, so the CAS-retry-loop technique
itself is the lesson, not memory reclamation.

## 2. The tools

### The CAS retry loop, recapped

`compare_exchange_weak(expected, desired)` atomically checks whether the atomic variable
still holds `expected`; if so, it becomes `desired` and the call returns `true`. If not, it
fails, refreshes `expected` to the *actual* current value, and returns `false`, the caller
is expected to loop and try again with that refreshed value. This is the same shape
[[015-treiber-stack]] and [[038-multithreaded-memory-pool]] both use for their free lists;
here it's a bucket's head pointer instead of a stack's top.

### Re-validating on every retry, not just once

The bug this question is built around is a general pattern, not specific to hash maps:
**any check that informs a CAS attempt has to be re-done on every retry, against that
retry's own snapshot of the shared state**, not computed once before the loop starts. A
retry means the world changed since your last look; whatever you checked before is stale the
moment `compare_exchange_weak` reports failure.

## 3. The broken version, first

The boilerplate checks for the key once, before the retry loop:

```cpp
for (Node* n = head.load(); n; n = n->next) if (n->key == key) return false; // once
Node* node = new Node{key, value, head.load()};
while (!head.compare_exchange_weak(node->next, node)) { /* never re-checks */ }
```

**Why it looks right:** read top to bottom, it's "check, then insert", exactly the correct
shape for a *single-threaded* insert, and the retry loop's job reads as purely mechanical
("keep trying until the atomic swap succeeds"), not as a second opportunity for the world to
have changed underneath the check that already happened.

Under `-DSHAKE`, this course's scheduling-perturbation build, widening exactly the window
between the check and the CAS, 10 keys, 40 threads racing to insert each one:

```
key 5: insert() returned true 2 times, expected exactly 1 (duplicate node in
the bucket if > 1)
```

**Worth naming explicitly: ThreadSanitizer's own 20 runs came back completely clean on this
file, no warning at all.** Every individual memory access here, every `load`, every
`compare_exchange_weak`, is a properly synchronized atomic operation; there is no data race
by the language's technical definition anywhere in this code. The bug is a *logical* race
across a sequence of individually-correct atomic operations, the same category of "TSan is
blind to this" bug this course has seen before ([[013-store-buffering]],
[[014-atomic-fences]]) but for a different reason, those were about *insufficient memory
ordering*; this is about a *stale check* spanning two separate atomic operations. Only the
functional, scheduling-perturbed stress test, checking the actual invariant ("exactly one
winner per key"), not just watching for a sanitizer warning, catches it.

The fix moves the check inside the loop:

```cpp
for (;;) {
    for (Node* n = old_head; n; n = n->next) if (n->key == key) { delete node; return false; }
    if (!node) node = new Node{key, value, old_head};
    if (head.compare_exchange_weak(old_head, node, ...)) return true;
}
```

Same 10-key, 40-thread-per-key contention test: `CLEAN` across the full sweep, compile,
12/12 correctness, 20 TSan runs, 150 shaken-stress runs, every one clean.

## 4. Real-world usage

**Why lock-free hash maps exist.** A mutex-per-bucket hash map is simple and, as section 5
shows, often perfectly competitive, but a lock has failure modes a CAS retry loop
structurally cannot: a thread holding a bucket's lock can be descheduled mid-critical-section
(paused by the OS at exactly the wrong moment), and every other thread wanting that bucket
now waits on a thread that isn't even running. A lock-free structure has no such state, any
thread that's actually scheduled can make progress, which matters most in contexts that
categorically forbid blocking (interrupt handlers, real-time paths, code that might already
hold the very lock a blocking wait would need).

**Where you meet the underlying technique:** Java's `ConcurrentHashMap`, `folly`'s (Meta's
C++ library) `ConcurrentHashMap`, and most production lock-free maps use exactly this
CAS-retry-on-bucket-head technique for insertion, the real engineering difference between
this exercise and a production implementation is almost entirely in what this exercise
deliberately scoped away: safe deletion (hazard pointers or epoch reclamation) and resizing
without blocking readers.

**Where NOT to reach for one:** a hash map with infrequent, low-contention writes and no
real-time or interrupt-context constraint gets little from lock-freedom and pays real
complexity for it, a plain `std::mutex`-per-bucket map (or even one global `std::shared_mutex`,
see [[051-shared-mutex-rwlock]]) is far simpler to get right, and section 5's measurement is
a direct reason not to assume the lock-free version is even faster by default.

## 5. Performance

800,000 inserts, 8 threads, 4096 buckets, this machine:

```
lock-free (CAS, linked-list buckets): 92-245 ms
mutex-per-bucket (std::vector buckets): 20-33 ms
```

The mutex version was **3-5x faster**, worth sitting with, because the honest full story
here is a methodology lesson, not just a number. This comparison varies **two** things at
once, not one: the synchronization primitive (CAS vs mutex) *and* the bucket's underlying
storage (a linked list of individually heap-allocated nodes vs a contiguous
`std::vector`). Both designs scan their bucket for a duplicate key before inserting, an
`O(bucket length)` cost either way, and average bucket length here is ~195 entries by the
end (800,000 inserts / 4096 buckets). A `std::vector` scan touches contiguous memory,
cache-friendly and fast; a linked list's scan chases pointers through scattered heap
allocations, a cache miss on essentially every `->next`. **The measured gap is very likely
dominated by cache locality, not by CAS-versus-mutex at all**, a fair, isolated comparison
would need both designs using the same bucket layout, which this measurement doesn't do.
The lesson: don't trust a benchmark that changes more than one thing between the "before" and
"after," even when, especially when, you wrote it yourself. And practically: "lock-free"
is not a synonym for "faster"; here, an unoptimized lock-free design measurably lost to a
plain mutex, for reasons that turned out to have little to do with locking at all.

## 6. Where this solution fails

- **This is genuinely insert-and-lookup only, there is no safe way to add delete without
 more machinery.** Freeing a node the instant it's unlinked is a use-after-free waiting to
 happen: another thread's `find()` may already be holding a pointer to it, mid-traversal.
 Real support for delete needs hazard pointers (each reader publishes which node it's
 currently examining, and a would-be deleter checks nobody's hazard pointer references a
 node before freeing it) or epoch-based reclamation (defer freeing until every thread has
 passed through a synchronization point proving it can't hold a stale reference), both
 real, substantial techniques this exercise deliberately doesn't build.
- **Fixed bucket count means unbounded key growth degrades into `O(n)` lookups per bucket**,
 exactly as section 5's ~195-entries-per-bucket average shows. A production map needs
 resizing, and resizing a lock-free structure without blocking every concurrent reader and
 writer is a much harder problem than anything this exercise covers.
- **Bucket-list traversal order is insertion order (most recent first), not any semantically
 meaningful order**, fine for this exercise's contract, but worth knowing if a caller ever
 assumed otherwise.
- **The duplicate-key re-check inside the retry loop makes each failed CAS attempt cost a
 fresh full bucket scan**, not just a fresh CAS, under very high contention on one specific
 key (many threads racing the same insert repeatedly), that's `O(bucket length)` work
 repeated on every single retry, which can get expensive exactly when contention is
 highest.

## 7. Interview follow-ups

**"Why didn't TSan catch the original bug, isn't that exactly what it's for?"** TSan's
model is happens-before relationships between individual memory accesses; every access here
(`load`, `compare_exchange_weak`) is already a properly synchronized atomic operation with no
ambiguity about ordering. The bug isn't in any single access, it's in the *gap in time*
between one atomic operation (the check) and a later, separate one (the CAS), a purely
logical staleness that no per-access race detector can see, because there's no race between
individual accesses to point at. This is exactly why this platform's stress/shake stage
exists as a distinct tool from TSan, not a redundant one.

**"How would you actually add safe deletion to this?"** Sketch hazard pointers concretely:
each thread maintains a small, publicly-visible array of "pointers I'm currently
dereferencing." Before a `find()` or traversal touches a node, it publishes that node's
address into its own hazard slot; before actually freeing an unlinked node, a deleter scans
every thread's hazard slots and only proceeds if none of them reference it, otherwise it
defers the free (adds the node to a "retire list" checked again later). This turns "is it
safe to free this" from "I hope nobody's looking" into an actual, checkable fact.

**"Given the measured result, would you actually recommend this lock-free design for a real
system?"** Not as measured, no, a fair comparison (same bucket layout) might change that
conclusion, but as it stands, a mutex-per-bucket map that's simpler to write, easier to
reason about, and *faster* on this measurement is the better default. Lock-free earns its
complexity specifically in contexts where blocking itself is unacceptable (interrupt
handlers, priority-inversion-sensitive real-time code), not as a general performance
optimization to reach for by default.
