Plain text, one paragraph per scenario naming the tool and the ruled-out alternative.
Example shape:

```
A. CPU flame graph. It's reproducible on demand and CPU is pegged at 100%, so
   the cost is computational, not waiting -- off-CPU would answer a question
   this scenario isn't asking.

B. A continuous/always-on profiler (Pyroscope/Parca-style), because nobody
   was capturing at 3:14am and it hasn't recurred -- you can't profile the
   past with a tool that wasn't running. Taking a fresh CPU or off-CPU
   capture now wouldn't catch anything, since the problem already stopped.

C. Differential flame graph, comparing last week's saved profile against a
   fresh one, because there's a known before/after boundary (the
   serialization change) and a "before" already exists. A plain CPU graph
   today has no baseline to compare against. Given reproduction is
   unreliable, I'd want a few captures (or continuous profiling over the
   rollout window) to rule out run-to-run noise.
```
