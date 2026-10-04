## ELI5: the kitchen goes silent for a second, for everyone

Every so often, the head chef yells "FREEZE" and every single person in the kitchen
stops mid-motion, including people who had nothing to do with whatever triggered the
freeze. Once the head chef finishes whatever needed everyone stopped for, work resumes
exactly where it left off. From any one cook's point of view, this looks like time just
vanished — they weren't slow, they weren't blocked on anyone, the whole kitchen simply
paused around them.

That's a stop-the-world garbage collection pause. It isn't a function in your call
stack being slow. It's every thread in the JVM being frozen at once so the collector can
safely move or reclaim memory, and then released.

## What you're actually building (understanding)

You profile a Java service and get a CPU flame graph that looks completely normal —
every visible frame is your own application code, nothing looks abnormally wide. And
yet real end-to-end request latency has unexplained gaps that don't correspond to
anything in the graph. Published benchmark comparisons of the three modern collectors
report roughly this shape of pause-time difference (real numbers vary by workload and
heap size — never take one published figure as gospel for your own service without
measuring it):

```
G1 (default since JDK 9):   ~48ms average pause, ~185ms at P99
Shenandoah (concurrent):    ~9.5ms average pause, ~18ms at P99
ZGC (concurrent, JDK 21+):  sub-millisecond pauses, largely independent of heap size
```

## Requirements

1. Why would a stop-the-world GC pause be completely invisible in a CPU flame graph of
   your application code, when it's clearly costing you real wall-clock time?
2. G1 does most of its work with the application still running, but still has real
   stop-the-world phases. ZGC and Shenandoah do far more of their work concurrently.
   What's the real cost of getting that concurrency — is it free, or does it trade
   against something?
3. A service with a small heap (2GB) and low allocation rate is currently on G1 and its
   P99 latency is fine. Would switching it to ZGC be a clear win, a likely non-event, or
   a real risk? Justify it instead of defaulting to "newer is always better."

## Why this matters

Most engineers' first instinct when latency has an unexplained gap is to keep staring
at the flame graph harder. If the cause is GC, the flame graph was never going to show
it — you need a different signal (GC logs, JFR's GC events) entirely.
