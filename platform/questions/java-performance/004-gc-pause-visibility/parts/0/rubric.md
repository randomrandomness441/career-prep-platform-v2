A good answer covers:

- **Why a CPU flame graph is blind to it.** A CPU profiler samples application threads
  that are actually running. During a stop-the-world pause, application threads are
  suspended — there's nothing on their stack to sample because they aren't executing at
  all. The GC's own work might show up under separate GC threads if the profiler captures
  those, but the *cost to your request* (waiting for the pause to end) never appears as a
  frame in your own code's call stack, because nothing in your code was running during it.
- **Concurrency isn't free.** ZGC and Shenandoah trade stop-the-world time for more
  background CPU work happening concurrently with the application (extra barriers on
  reads/writes to catch a moving object, more total CPU spent on bookkeeping) — lower
  pause times generally cost some amount of throughput or extra CPU overhead compared to
  a collector that does more of its work in one uninterrupted burst. A good answer names
  this as a real tradeoff, not a strictly-better replacement.
- **The small-heap, low-allocation case.** Likely a non-event or even a real risk, not a
  clear win — ZGC's main advantage (pause times that don't grow with heap size) matters
  most for large heaps where G1's pauses get worse as the heap grows. A 2GB heap with low
  allocation pressure is close to G1's best case already; switching collectors adds
  operational risk (a less-tuned, less-familiar collector) for a benefit that may not be
  measurable on this specific workload. NEEDS_WORK if the answer defaults to "ZGC is
  strictly better, switch to it" without engaging with the specific workload described.

NEEDS_WORK if the answer thinks a CPU flame graph would show GC pause time directly, or
treats collector choice as risk-free.

## Configuration

This question is about collector choice, not application code — the "wrong vs right"
lives in JVM flags.

**Reaching for a fresh CPU capture when the gap is GC, not compute:**
```
# Won't show the missing time -- GC pauses never appear in a CPU profile at all
async-profiler -e cpu -d 30 -f cpu.svg <pid>
```

**Correct — look at GC logs / JFR's GC events directly:**
```
-Xlog:gc*:file=gc.log:time,uptime,level,tags
```

**Alternative — switching collectors only where the workload actually justifies it (large heap, pause-sensitive):**
```
# G1 (default) -- fine for the 2GB/low-allocation case in this question
-XX:+UseG1GC

# ZGC -- worth it once heap size and pause sensitivity actually demand it
-XX:+UseZGC -Xmx64g
```

