Plain text, three numbered points. Example shape:

```
1. expensiveCompute has no side effects at all -- if its result is never
   read, the JIT can prove the entire call has no observable effect on the
   program, so eliminating the call and eliminating the discarded
   assignment are the same optimization applied consistently.
2. The print already happens after timing stops, so an unconditional print
   wouldn't cost anything inside the measured region either way. The real
   reason for the guard is that the same method runs during warm-up calls
   too -- printing unconditionally would spam output every warm-up
   iteration, not that it protects the timing itself.
3. Whether the result is consumed by something the JIT can't prove is
   side-effect-free, and whether the benchmark actually warmed up before
   the measured region. A suspiciously fast number is more likely to mean
   "nothing happened" than "something got optimized well."
```
