Plain text, three numbered points. Example shape:

```
1. During a stop-the-world pause, application threads are suspended -- there's
   nothing on their stack to sample. A CPU profiler samples running threads;
   a frozen thread has no frame to attribute the cost to.
2. Not free. Concurrent collectors do more bookkeeping and add barriers on
   reads/writes to track objects while the app keeps running, trading some
   throughput/CPU overhead for shorter pauses.
3. Likely a non-event or a real risk, not a clear win. ZGC's advantage is
   pauses that don't grow with heap size -- a small heap with low allocation
   pressure is already close to G1's best case, so switching adds
   operational risk for a benefit that may not show up here.
```
