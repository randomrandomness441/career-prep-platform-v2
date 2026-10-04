## 1. Reframe

"Java is garbage collected, so it doesn't leak" is only true for the memory the
collector actually manages. Off-heap memory has real, separate lifetime rules that
depend on either a Java wrapper object's own collection (indirect, and easy to get
wrong) or explicit native cleanup that may simply never be called.

## 3. The broken version, first

Someone investigating a slow, steady OOM kill reaches for a heap dump first — the most
familiar tool, the one that's solved every other memory problem they've seen — finds
nothing wrong, and concludes the leak must be somewhere mysterious or gives up
diagnosing it as a memory leak at all, maybe blaming the container's memory limit
instead. The heap dump was never going to show this class of problem; it was the wrong
tool for this specific question from the start, not a sign the investigation is on the
wrong track.

## 4. Interview follow-ups

- Why does a `DirectByteBuffer`'s native memory only get released when the *Java
  wrapper object* is collected, rather than immediately when the buffer is done being
  used? Because the JVM ties the native memory's lifetime to the Java object's garbage
  collection by design (a `Cleaner`/finalization mechanism runs when the wrapper becomes
  unreachable) — if the wrapper object is still reachable (cached, held in a collection
  longer than intended), the native memory behind it stays alive too, even if nothing is
  actually using it anymore.
- Question 013 covered G1 humongous objects needing contiguous heap regions. Is there a
  connection between large `DirectByteBuffer` usage and heap fragmentation? Not directly
  in the same way — direct buffers live off-heap, so they don't consume G1 regions at
  all. But a service allocating many large direct buffers frequently can still see real
  memory pressure and fragmentation, just in the OS-level native heap rather than the
  JVM's managed heap — the mechanism is different, but "large allocations create
  different-shaped memory problems than small ones" is the shared lesson.
