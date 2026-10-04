## 1. Reframe

Not all sampling bias comes from your choice of sampling rate. Some of it is baked into
how the profiler is allowed to look at the program at all — the JVM's own safepoint
mechanism decides when a profiler is even permitted to ask "what's running right now."

## 3. The broken version, first

Someone profiles a service with JFR, sees method A ranked far above method B, and
optimizes A first because the profile said so. If A happens to be I/O-heavy (frequent
natural safepoints) and B is a tight computational loop (rare safepoints), the ranking
itself can be an artifact of the profiler's blind spot, not a reflection of real cost —
optimizing the profile's top entry isn't the same as optimizing the real bottleneck when
the measurement tool has a structural bias like this one.

## 4. Interview follow-ups

- Has this gotten better in newer JDKs? Yes — JDK 21+ partially reduces safepoint bias
  in JFR, and JDK 25 introduced a CPU-time sampler in JFR designed to avoid it entirely,
  explicitly marking any remaining safepoint-biased samples so they're identifiable
  rather than silently mixed in with unbiased ones.
- `async-profiler`'s approach combines two different mechanisms — `AsyncGetCallTrace`
  for Java frames and `perf_events` for native/kernel frames — why not just use one?
  Neither alone gives the full picture: `AsyncGetCallTrace` only knows about Java-level
  stack frames, and `perf_events` alone can't resolve JIT-compiled Java method names
  without help (the same JIT-symbol problem covered in the base pack's frame-pointer
  reading). Combining both is what makes a complete, accurate mixed-mode Java profile
  possible.
