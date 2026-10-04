## 1. Reframe

A CPU flame graph only ever shows you time your own threads spent running. GC pause
time is, by definition, time your threads spent *not* running. It's a structurally
different kind of cost from anything covered in the base flame graph pack's CPU-only
lessons, closer in spirit to off-CPU time than to a hot function.

## 3. The broken version, first

Someone chasing an unexplained latency gap keeps re-profiling the same CPU flame graph,
looking harder at frames that were never going to contain the answer. The tell that
should redirect them: total request time doesn't add up to the sum of the visible
frames' widths. If the numbers don't reconcile, the missing time is happening somewhere
the CPU profiler structurally can't see — GC logs (`-Xlog:gc`) or JFR's GC events are
the actual next place to look, not a longer or higher-resolution CPU capture.

## 4. Interview follow-ups

- G1 humongous object allocation (any object over half a region's size) bypasses the
  young generation and goes straight into old-gen regions, fragmenting the heap and
  triggering extra GC cycles. If a service that allocates unusually large arrays or
  buffers has worse-than-expected GC overhead despite short-lived objects, what would
  you check? Whether those large allocations are being classified as humongous, and
  whether increasing `-XX:G1HeapRegionSize` moves them back into the normal allocation
  path.
- What's the difference between "GC is slow" and "the application is allocating too
  much, too fast"? A collector spending 40% of wall time on GC isn't necessarily a badly
  tuned collector — it can be an application creating far more garbage than necessary
  (unneeded object churn, wide autoboxing, defensive copies), and no amount of GC tuning
  fixes an allocation-rate problem at its source.
