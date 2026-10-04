A good answer covers:

- **Yes, they bypass young gen, and that's expensive.** 1.5MB is over the 1MB humongous
  threshold on this configuration, so every one of these "short-lived" buffers skips the
  young generation and goes straight into old-gen humongous regions. A good answer names
  the real implication: being short-lived normally means cheap in a generational
  collector (young-gen collections are fast and frequent by design), but a humongous
  object doesn't get that benefit even if its actual lifetime is just as short — it's
  reclaimed through old-gen/mixed collection cycles instead, which are less frequent and
  more expensive per object processed.
- **Why fragmentation specifically.** Humongous objects need *contiguous* regions —
  unlike normal objects that can go anywhere with space, a humongous allocation needs an
  unbroken run of free regions large enough to hold it. Repeated humongous allocation and
  reclamation leaves irregular gaps that a later humongous allocation may not fit into
  even if the *total* free space is enough, forcing a full/compacting collection to
  consolidate space — a cost pattern that ordinary small-object churn, which can use any
  scattered free space, doesn't create in the same way.
- **A real lever with no code change.** Increasing `-XX:G1HeapRegionSize` explicitly
  raises the humongous threshold (since it's defined as half the region size) — objects
  that were humongous at a 2MB region size may no longer be humongous at a 4MB or 8MB
  region size, moving them back onto the normal allocation path without touching the
  application at all. A good answer names this flag specifically, not just "tune the
  JVM" vaguely.

NEEDS_WORK if the answer thinks short-lived automatically means cheap regardless of
size, or can't name the region-size flag as a concrete lever.

## Code

**Inefficient — a fresh 1.5MB buffer allocated and discarded on every request:**
```java
byte[] processRequest(Request r) {
    byte[] buf = new byte[1_500_000];   // humongous every time, bypasses young gen
    fill(buf, r);
    return compress(buf);
}
```

**Correct — pool and reuse the buffer instead of allocating fresh each time:**
```java
private final ArrayBlockingQueue<byte[]> pool = new ArrayBlockingQueue<>(16);

byte[] processRequest(Request r) {
    byte[] buf = pool.poll();
    if (buf == null) buf = new byte[1_500_000];   // allocated once, reused after
    try {
        fill(buf, r);
        return compress(buf);
    } finally {
        pool.offer(buf);
    }
}
```

**Alternative — no code change, raise the humongous threshold itself:**
```
-XX:G1HeapRegionSize=4m    # 1.5MB buffers now fit the normal (non-humongous) path
```

