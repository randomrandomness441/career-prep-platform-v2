Plain text, three numbered points. Example shape:

```
1. A GC pause is exactly when many GC worker threads try to run
   simultaneously -- that burst of demand collides hardest with a tight CPU
   quota, burning the whole scheduling period's allowance almost instantly.
2. Whether the CPU limit is fractional (rounds oddly) or effectively unset,
   and whether -XX:ActiveProcessorCount should be set explicitly rather than
   trusting auto-detection -- UseContainerSupport being on isn't a complete
   guarantee by itself.
3. A gap, not high usage. Throttled time is off-CPU time from the scheduler's
   perspective -- the process briefly spikes to 100%+ of quota then gets cut
   off entirely for the rest of the period. Same invisible shape as a GC
   pause: a CPU flame graph over that window shows low/gapped usage, not
   sustained high usage.
```
