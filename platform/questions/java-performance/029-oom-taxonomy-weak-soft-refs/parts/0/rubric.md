A good answer covers:

- **Why more heap doesn't fix a native-thread OOM.** "Unable to create new native
  thread" comes from the *operating system* refusing to create a new OS-level thread —
  typically because the process has hit an OS limit (max processes/threads per user, or
  physical/virtual memory exhausted by the *sum* of every thread's stack allocation, not
  by heap objects). The managed Java heap and this resource are unrelated; increasing
  `-Xmx` changes nothing about how many OS threads the kernel will grant the process. A
  good answer names the real likely cause: too many threads created (often a leak —
  threads started and never stopped or pooled) relative to what the OS or a configured
  ulimit allows.
- **The soft-reference cache isn't broken.** A `SoftReference` is only cleared under
  actual memory pressure — holding onto everything for days on a server with plenty of
  free RAM is exactly the documented, intended behavior, not a bug. What would actually
  be worth worrying about: if the cache keeps growing *unbounded* even as the JVM
  legitimately runs low on memory and the soft references still aren't being cleared —
  that would suggest something else is holding strong references to the same objects,
  defeating the point of using soft references at all.
- **Why `WeakReference` produces a different, worse surprise for a cache.** A
  `WeakReference` gets cleared the moment nothing else holds a strong reference — with no
  regard for whether memory is actually needed. For a cache, that means entries can
  disappear essentially immediately after being inserted, the instant nothing else
  happens to be holding the value strongly, making the cache far less effective than
  intended — not a memory-pressure-driven eviction policy at all, but an
  as-soon-as-possible one. A good answer names this as "far too eager, not tied to
  actual memory pressure" rather than just "different."

NEEDS_WORK if the answer recommends increasing heap size for a native-thread OOM, or
treats a long-lived soft-reference cache under low memory pressure as evidence of a bug.

## Code

**Inefficient — a strong-referenced cache that only ever grows, real heap-space OOM risk:**
```java
private final Map<String, byte[]> cache = new HashMap<>();   // never evicts, strong refs

byte[] get(String key) {
    return cache.computeIfAbsent(key, this::loadFromDisk);   // grows forever
}
```

**Correct — SoftReference values, eligible for collection specifically under real memory pressure:**
```java
private final Map<String, SoftReference<byte[]>> cache = new ConcurrentHashMap<>();

byte[] get(String key) {
    SoftReference<byte[]> ref = cache.get(key);
    byte[] value = (ref != null) ? ref.get() : null;
    if (value == null) {
        value = loadFromDisk(key);
        cache.put(key, new SoftReference<>(value));
    }
    return value;
}
```

**Wrong tool for a cache — WeakReference evicts far too eagerly, not tied to memory pressure at all:**
```java
private final Map<String, WeakReference<byte[]>> cache = new ConcurrentHashMap<>();
// entries can vanish the instant nothing else happens to hold a strong reference --
// use WeakReference for identity-keyed lookups (e.g. WeakHashMap), not for caching
```

