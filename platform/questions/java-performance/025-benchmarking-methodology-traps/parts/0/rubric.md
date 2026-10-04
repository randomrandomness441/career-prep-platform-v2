A good answer covers:

- **Why the whole call gets deleted, not just the assignment.** `expensiveCompute` has
  no side effects — it doesn't mutate any field, doesn't do I/O, doesn't throw in a way
  that's observable differently based on input. If its result is never read, the JIT can
  prove the entire call (all 50 inner iterations) has no effect on any observable
  program behavior, so removing the discarded assignment and removing the call that
  produced the discarded value are the same optimization, applied consistently — a good
  answer states that "no observable effect" is what licenses eliminating the *whole*
  computation, not just the storage of its result.
- **Where the print actually needs to live, and why the guard is used anyway.** The
  print in the fix happens *after* timing stops (`long ms = ...` is computed first) —
  so an unconditional, always-printed `sink` would cost nothing extra inside the timed
  region either way. The real, practical reason to guard it behind an all-but-impossible
  condition is that this same method gets called during warm-up too (many times, not
  just once) — printing unconditionally would spam output on every warm-up call. A good
  answer doesn't overstate this as protecting the *timing measurement itself*; the
  measurement is already safe either way, since the print sits outside the timed window.
- **What to check in someone else's benchmark.** Whether the computed result is ever
  consumed by something the JIT can't prove is side-effect-free (a field write, I/O, a
  value fed into a later, real computation) — and, more broadly, whether the benchmark
  warmed up before the measured region, since an un-warmed benchmark measures
  interpreter/early-JIT behavior rather than steady state. A good answer names checking
  for dead-code-elimination risk specifically as the first, most likely way a suspiciously
  fast number could be meaningless rather than impressive.

NEEDS_WORK if the answer thinks the print needs to be unconditional to protect the
timing measurement, or can't explain why the JIT is licensed to remove the whole call,
not just the assignment.

## Code

**Broken — result discarded, the JIT is free to eliminate the whole computation:**
```java
long t0 = System.nanoTime();
for (int i = 0; i < n; i++) {
    expensiveCompute(i);          // return value never used
}
long ms = (System.nanoTime() - t0) / 1_000_000;   // measures ~nothing
```

**Correct — result consumed, the computation can't be proven side-effect-free:**
```java
double sink = 0;
long t0 = System.nanoTime();
for (int i = 0; i < n; i++) {
    sink += expensiveCompute(i);
}
long ms = (System.nanoTime() - t0) / 1_000_000;
if (sink == Double.MIN_VALUE) System.out.println("unreachable " + sink);   // outside the timed region
```

**Alternative — use a real microbenchmark harness instead of hand-rolling the defenses:**
```java
@Benchmark
public double bench(Blackhole bh) {
    return expensiveCompute(42);   // JMH's @Benchmark return value is auto-consumed
}
```

