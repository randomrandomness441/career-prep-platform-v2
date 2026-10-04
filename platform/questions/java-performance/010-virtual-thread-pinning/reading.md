## 1. Reframe

Virtual threads solve the old problem of "too many OS threads is expensive" by making
threads cheap and letting many of them share a small pool of real carriers. Pinning is
the failure mode of that exact trick: a case where a virtual thread can't be swapped
off its carrier when it should be able to, silently turning "thousands of cheap
threads" back into "a handful of expensive threads" for exactly the code paths that hit
it.

## 3. The broken version, first

Someone migrates a service from platform threads to virtual threads expecting a free
scalability win, leaves existing `synchronized` blocks untouched because "it still
compiles and passes tests," and sees no problem in local testing (low concurrency,
pinning rarely coincides across enough requests to matter). Under real production load
— many concurrent requests hitting the same `synchronized`-guarded blocking call at
once — throughput collapses in a way that looks like it could be almost anything else
(thread pool starvation, a slow downstream dependency, GC) until someone specifically
checks for pinning.

## 4. Interview follow-ups

- Why doesn't this affect a virtual thread blocked on a plain, unsynchronized I/O call
  (a regular blocking `HttpClient` request, no `synchronized` involved)? Because the
  Java standard library's own blocking I/O operations are specifically written to be
  virtual-thread-aware and unmount cleanly — pinning specifically comes from older
  synchronization primitives (`synchronized`, and some legacy JNI-based blocking calls)
  that predate virtual threads and weren't designed with unmounting in mind.
- If a team is stuck on JDK 21 for a while and can't quickly hunt down every
  `synchronized` block, is there a way to at least detect the blast radius without
  fixing every site immediately? Turn on continuous JFR recording with the
  `jdk.VirtualThreadPinned` event enabled in production, which surfaces every pin site
  and its duration over real traffic — this doesn't fix anything, but converts "we don't
  know how bad this is" into a concrete, prioritizable list of exactly which call sites
  are actually costing throughput, so the team can fix the worst ones first instead of
  guessing.
