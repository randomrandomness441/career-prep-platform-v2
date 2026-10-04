Plain text, three numbered points. Example shape:

```
1. It would under-report the tight loop's real cost and over-report the
   I/O-heavy method relative to it -- the profile's ranking of which method
   costs more could be inverted from what's actually true.
2. More safepoints means more frequent moments where every thread has to
   reach a checkpoint before any can proceed -- a constant coordination tax
   paid on every run, not just while profiling, to fix something that only
   matters during profiling.
3. JFR is built into the JDK, low overhead, safe to run continuously in
   production, and well-supported by tooling. async-profiler needs a
   separately installed native agent and relies on an internal, unsupported
   API -- a team prioritizing operational simplicity might stick with JFR
   despite the bias.
```
