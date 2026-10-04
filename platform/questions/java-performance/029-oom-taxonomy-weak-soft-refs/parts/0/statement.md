## ELI5: "the building is full" means different things in different buildings

"Sorry, we're full" means something different at a hotel (no more rooms) than at a
parking garage (no more spaces) than at a restaurant (no more tables) — same sentence,
completely different resource, completely different fix. Telling someone "the building
said it's full" without saying which building doesn't help them do anything about it.

`OutOfMemoryError` is one exception class with several genuinely different real causes,
distinguishable by its actual message — and the fix for one has nothing to do with the
fix for another.

## What you're actually building (understanding)

Triggered for real, on this machine, JDK 21, a small heap and a loop that keeps
allocating without ever releasing anything:

```
Exception in thread "main" java.lang.OutOfMemoryError: Java heap space
	at OomHeap.main(OomHeap.java:9)
```

That specific message — "Java heap space" — means exactly one thing: live objects plus
garbage the collector hasn't reclaimed yet don't fit in the heap. Several other real,
distinct `OutOfMemoryError` messages exist, each pointing at a completely different
resource: **GC overhead limit exceeded** (the collector is running constantly but
reclaiming almost nothing — a symptom, not of "not enough heap" alone, but of a
workload where GC can never keep up), **Metaspace** (too many loaded classes, not too
many objects), **Unable to create new native thread** (the *operating system* refused a
new OS thread — an OS-level limit, not a JVM heap problem at all), and **Direct buffer
memory** (native memory behind `DirectByteBuffer`s, question 016's topic, exhausted —
completely separate from the managed heap).

Separately: `WeakReference` and `SoftReference` both let an object be collected even
while something still technically points to it, but on different terms. A `Weak`
reference is cleared as soon as the garbage collector determines nothing else holds a
strong reference to the object — it doesn't wait for memory pressure at all. A `Soft`
reference is only cleared when the JVM actually needs the memory, which means a
soft-referenced cache can legitimately hold onto everything for a very long time if
memory isn't tight.

## Requirements

1. A service logs `OutOfMemoryError: Unable to create new native thread`. Why would
   adding more heap memory (`-Xmx`) do absolutely nothing to fix this, and what's the
   real, likely cause?
2. A cache implemented with `SoftReference` values is meant to "automatically free
   memory under pressure." A team observes it still holding millions of entries after
   running for days on a server with plenty of free RAM, and worries something is
   broken. Is it broken? What would make it reasonable to actually worry?
3. Why does using `WeakReference` instead of `SoftReference` for the same cache produce
   a completely different, and usually undesirable, kind of surprise?

## Why this matters

"Just add more memory" is the correct fix for exactly one of these error messages and
the wrong fix for the others — reading the actual message, not just the exception class
name, is the entire difference between fixing the real problem and wasting a deploy
cycle on a change that can't help.
