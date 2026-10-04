Plain text, three numbered points. Example shape:

```
1. Measure actual allocation rate (bytes/sec or objects/sec) via an
   allocation profile or GC logs showing collection frequency, and compare
   against what the workload should plausibly need. Frequent collections
   with normal pause sizes points at allocation rate; long individual pauses
   at normal frequency points more at tuning.
2. GC tuning changes how efficiently the collector processes a given volume
   of garbage, not how much garbage gets produced. There's a floor on total
   GC work no flag changes -- tuning can redistribute the cost, not shrink
   the underlying volume.
3. GC tuning feels like the expert move and directly addresses the metric
   everyone's staring at (GC time %). Looking at application allocation
   feels like a step back once a team is already framing it as a GC problem
   -- the dashboard metric itself points attention at the collector even
   when the collector was never the actual issue.
```
