## 1. Reframe

`OutOfMemoryError` is a class name, not a diagnosis. The actual message after the colon
names a specific, distinct resource, and the fix for one has nothing to do with the fix
for another — treating them as interchangeable wastes real time chasing the wrong cause.

## 2. What was actually verified, for real

Triggered directly on this machine, JDK 21, a small heap and an unreleased allocation
loop:

```
Exception in thread "main" java.lang.OutOfMemoryError: Java heap space
```

The other message variants named in this question (GC overhead limit exceeded,
Metaspace, Unable to create new native thread, Direct buffer memory) are real,
documented HotSpot `OutOfMemoryError` messages, each pointing at a genuinely different
resource than plain heap space.

## 3. The broken version, first

Every one of these error messages shares the same exception class name,
`java.lang.OutOfMemoryError`, which makes it tempting to treat "OOM" as a single problem
with a single standard fix (usually: add more heap). For the native-thread and Metaspace
variants specifically, that fix does nothing at all — the resource that ran out was
never the heap.

## 4. Interview follow-ups

- Question 016 covered off-heap/native memory leaks being invisible to a heap dump.
  Does "Direct buffer memory" OOM relate to that? Directly — it's the specific error
  that fires when the reserved capacity for `DirectByteBuffer` allocations (a separately
  configurable limit, `-XX:MaxDirectMemorySize`) is exhausted, which is exactly the kind
  of native, off-heap growth a plain heap dump can't see coming.
- "GC overhead limit exceeded" specifically means the collector ran repeatedly and
  reclaimed very little each time — why is this a distinct signal from plain "Java heap
  space," even though both are ultimately about not enough usable heap? Plain heap space
  exhaustion is a single failed allocation with nothing left to give. GC overhead limit
  exceeded is a *pattern* the JVM detects over multiple collections — spending an
  excessive proportion of total time collecting for a shrinking return — which usually
  points more specifically at a workload with sustained memory pressure just below full
  exhaustion, rather than one final allocation that simply didn't fit.
