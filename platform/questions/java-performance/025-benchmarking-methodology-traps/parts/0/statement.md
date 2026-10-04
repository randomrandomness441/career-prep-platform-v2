## ELI5: bragging about a race you didn't actually run

Imagine timing yourself running a mile, except partway through you realize nobody's
watching, so you just stand still and write down a fast-looking number. The stopwatch
reading is real. The race never happened. Anyone trusting your number without checking
whether you actually ran would walk away believing something false — not because you
lied about the *time*, but because there was nothing real behind it.

A Java benchmark can do this to you automatically, without anyone intending it. If the
JIT can prove a computation's result is never used for anything observable, it's free
to skip the computation entirely — and it will, because that's a completely correct
optimization from its point of view. Your stopwatch still reads a real, small number.
The work never happened.

## What you're actually building (understanding)

Real, measured on this machine, JDK 21 — two versions of the exact same expensive
computation, called the same number of times, differing only in whether the result is
ever consumed:

```java
static double expensiveCompute(double x) {
    double r = x;
    for (int i = 0; i < 50; i++) r = Math.sqrt(r * r + 1.0);
    return r;
}

// BROKEN: result discarded
for (int i = 0; i < n; i++) { expensiveCompute(i); }

// FIXED: result feeds into something observable
double sink = 0;
for (int i = 0; i < n; i++) { sink += expensiveCompute(i); }
if (sink == Double.MIN_VALUE) System.out.println("unreachable " + sink);
```

```
broken (result discarded): 7ms
fixed  (result consumed):  6127ms
```

**~875x.** The "broken" version isn't measuring a fast optimization. It's measuring
almost nothing — the JIT proved the entire loop had no observable effect and eliminated
it.

## Requirements

1. Why is discarding a pure function's return value enough for the JIT to justify
   deleting the *entire* call, including all 50 iterations of the inner loop, rather
   than just skipping the assignment?
2. The fix here uses an accumulator (`sink`) checked against an all-but-impossible
   condition (`Double.MIN_VALUE`) before printing. Why not just always print `sink` at
   the end of the benchmark unconditionally — what would that cost, and why might a
   benchmark author want to avoid paying it inside the timed region?
3. This exact platform's own questions (009, 018, 019, 022) all used a `sink`,
   accumulator, or printed result specifically to avoid this trap when measuring real
   JVM behavior. Given what you now know, what would you specifically check for if
   someone handed you a Java microbenchmark result and asked whether to trust it?

## Why this matters

Every real number in this entire pack was measured by deliberately avoiding this exact
trap. A benchmark result that looks impossibly good is not evidence of a fast
optimization — it's usually the first thing worth checking for before believing
anything else about the result.
