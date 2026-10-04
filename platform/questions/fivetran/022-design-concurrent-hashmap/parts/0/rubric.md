A good answer covers:

- **Names where a naive hash map actually breaks under concurrency.** Two threads writing
  to the same bucket's linked list at the same time can corrupt the list itself, not just
  overwrite each other's value. A reader walking that list during a concurrent write can
  follow a half-updated pointer into garbage. This has to be named specifically, not just
  asserted as "it's not thread-safe."
- **Rejects a single global lock with a real reason, not just "it's slow."** One lock
  around every operation is correct and simple. It's bad because it serializes unrelated
  operations. Two threads touching completely different keys still queue behind each
  other, so the map can't use more than one core no matter how many threads call it.
- **Proposes lock striping (or an equivalent) and explains why it works.** Split the
  bucket array into N segments, each with its own lock. An operation only takes the lock
  for the segment its key hashes into. Two keys in different segments proceed in parallel.
  A good answer states the real tradeoff: N locks means N-way parallelism at best, and
  picking N too small still serializes, too large wastes memory on lock objects.
- **Handles resize correctly, not just "lock everything and resize."** This is where most
  naive designs break, and the question asks for it directly. A good answer recognizes
  that resize has to either grab every segment's lock before touching the array, or use
  a scheme where the old and new arrays coexist for a transition period (incremental
  resize, as `ConcurrentHashMap` actually does) so no single resize call blocks every
  thread in the system for the duration.
- **States what a concurrent `get` actually guarantees, and knows it's not linearizable
  by default.** A `get` that doesn't take any lock can run concurrently with a `put` on
  a different key with no issue, and concurrently with a `put` on the *same* key with a
  real question: does it see the old value, the new value, or is that undefined without
  a `volatile`/memory-barrier guarantee on the bucket reference? A good answer says this
  explicitly rather than assuming "it just works."
- **Has a real answer for "how would you verify this,"** not "write some tests." Stress
  testing with many threads hammering overlapping keys, running under a race detector
  (ThreadSanitizer or Java's own happens-before checkers), and reasoning explicitly about
  which operations need to happen-before which others, are all legitimate answers. "I'd
  write unit tests" alone is not enough at this level, because ordinary unit tests run
  single-threaded and can't see most of these bugs.

NEEDS_WORK if the answer jumps straight to "use `ConcurrentHashMap`" without ever
explaining what it does internally, or if resize safety is skipped entirely, or if the
answer claims a global lock is "just as good" without naming the parallelism it gives up.

## Illustrative sketch (lock striping, not what the candidate has to write)

```java
class StripedMap<K, V> {
    private final Object[] locks;
    private volatile Entry<K, V>[] buckets;   // volatile: a resize swaps this reference

    private Object lockFor(K key) {
        return locks[(key.hashCode() & 0x7fffffff) % locks.length];
    }

    V get(K key) {
        // no lock: may race with a concurrent put on the same key -- that's the
        // tradeoff a good answer should name, not hide.
        for (Entry<K, V> e = buckets[bucketIndex(key)]; e != null; e = e.next) {
            if (e.key.equals(key)) return e.value;
        }
        return null;
    }

    void put(K key, V value) {
        synchronized (lockFor(key)) {
            // only this key's stripe is blocked -- a put on a key in a different
            // stripe proceeds concurrently, which is the entire point.
            ...
        }
    }

    void resize() {
        // naive: synchronized on every lock in `locks` before swapping `buckets`.
        // real ConcurrentHashMap instead migrates incrementally so no single call
        // blocks every other thread for the whole table.
    }
}
```
