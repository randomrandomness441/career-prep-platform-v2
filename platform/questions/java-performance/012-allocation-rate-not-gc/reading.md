## 1. Reframe

"High GC overhead" is a measurement of a symptom, not a diagnosis. The collector can be
correctly doing exactly what it should with an unreasonable amount of garbage handed to
it — the fix in that case lives in the application, not the JVM flags.

## 3. The broken version, first

A week spent tuning `-XX:MaxGCPauseMillis`, heap sizes, and collector choice against a
self-inflicted allocation problem produces marginal, disappointing gains no matter how
carefully it's done — because the tuning was never wrong, the premise was. The actual
fix (question 012's example: one unnecessary defensive copy removed) took less time to
find once allocation was profiled directly than the GC tuning did, and fixed the root
cause instead of redistributing its symptoms.

## 4. Interview follow-ups

- Is there a rule of thumb for "this allocation rate is definitely too high"? Not a
  universal one — it depends entirely on the workload and hardware. The useful
  comparison isn't against an absolute number, it's against your own service's own
  history: a sudden, unexplained jump in allocation rate after a deploy is a much
  stronger signal than any fixed threshold.
- Question 004 covered GC pause visibility being invisible to a CPU flame graph. Does an
  allocation flame graph suffer the same blind spot? No — allocation profiling samples a
  different event entirely (allocations, typically via TLAB sampling) and isn't affected
  by whether the application thread was paused during a stop-the-world GC; it directly
  answers "where did this garbage come from," which a CPU profile structurally can't.
