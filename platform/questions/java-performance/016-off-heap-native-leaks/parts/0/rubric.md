A good answer covers:

- **The first suspect category.** Off-heap memory broadly — a heap dump only accounts
  for managed heap objects; a growing gap between heap usage (stable, as reported) and
  RSS (climbing) is close to a textbook signature that the growth is happening somewhere
  the heap dump structurally cannot see: `DirectByteBuffer`-backed native memory, JIT
  code cache, thread stacks, or native/JNI allocations. A good answer names off-heap
  memory as a category first, before jumping straight to a specific mechanism.
- **NMT's actual coverage.** NMT would help with the `DirectByteBuffer` case — buffer
  allocation goes through the JVM itself, so NMT can account for it. NMT would miss the
  raw JNI `malloc` case entirely — a native library allocating memory directly, outside
  any JVM API, is invisible to a tool that only tracks the JVM's own allocations. A good
  answer states this distinction explicitly and explains *why* (whether the allocation
  path goes through the JVM or bypasses it entirely), not just which tool "works better."
- **A concrete technique for the JNI case.** Real options: `async-profiler` with native
  allocation/malloc tracing enabled (surfacing native allocation call stacks the same
  way it surfaces Java ones), or swapping the process's memory allocator for one with
  built-in profiling support (jemalloc's profiling mode is a real, named example) to
  capture native allocation stacks directly from outside the JVM's own visibility.
  General OS-level tools (checking process RSS growth against `/proc` mappings on
  Linux) are also acceptable as a first triage step, though they identify *that* it's
  growing rather than *where* in the native code it's coming from.

NEEDS_WORK if the answer treats NMT as covering all non-heap memory uniformly, or can't
name a real technique for the JNI-specific blind spot.

## Code

**Inefficient — a direct buffer allocated per call, wrapper object dropped, native memory lingers:**
```java
ByteBuffer encode(byte[] data) {
    ByteBuffer buf = ByteBuffer.allocateDirect(data.length);
    buf.put(data);
    return process(buf);   // caller discards it; native memory waits for GC of the wrapper
}
```

**Correct — pool and reuse direct buffers instead of allocating fresh ones:**
```java
private final Queue<ByteBuffer> pool = new ConcurrentLinkedQueue<>();

ByteBuffer encode(byte[] data) {
    ByteBuffer buf = pool.poll();
    if (buf == null) buf = ByteBuffer.allocateDirect(64 * 1024);
    buf.clear().put(data);
    ByteBuffer result = process(buf);
    pool.offer(buf);          // returned for reuse, native memory stays bounded
    return result;
}
```

**Alternative — explicit, immediate release instead of waiting on GC, when reuse isn't practical:**
```java
ByteBuffer buf = ByteBuffer.allocateDirect(data.length);
try {
    buf.put(data);
    return process(buf);
} finally {
    ((DirectBuffer) buf).cleaner().clean();   // frees native memory now, not at GC's discretion
}
```

