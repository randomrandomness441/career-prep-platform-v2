Plain text, three numbered points. Example shape:

```
1. Off-heap memory broadly -- a heap dump only sees managed heap objects. A
   stable heap with climbing RSS is close to the textbook signature that
   growth is happening somewhere the dump structurally can't see:
   DirectByteBuffers, code cache, thread stacks, or native/JNI allocations.
2. NMT helps with the DirectByteBuffer case, since buffer allocation goes
   through the JVM itself. It misses the raw JNI malloc case entirely --
   a native library allocating outside any JVM API is invisible to a tool
   that only tracks the JVM's own allocations.
3. async-profiler with native allocation/malloc tracing enabled, or swapping
   in an allocator like jemalloc with profiling support turned on, to
   capture native allocation stacks from outside the JVM's own visibility
   entirely.
```
