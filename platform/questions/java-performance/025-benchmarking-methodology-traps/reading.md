## 1. Reframe

A benchmark result isn't automatically evidence of anything. It's a measurement of
whatever code actually ran, and the JIT is fully licensed to make "whatever code
actually ran" be nothing at all, if it can prove the result was never going to matter.

## 2. What was actually measured, for real

JDK 21, this machine, the exact same `expensiveCompute` call site, same iteration
count, differing only in whether the return value is consumed:

```
broken (result discarded): 7ms
fixed  (result consumed):  6127ms
```

7ms for 50 million calls, each doing 50 iterations of real floating-point math, is not
plausible as real work — it's a tell, once you know to look for it, that nothing
actually happened.

## 3. The broken version, first

Someone benchmarking a candidate optimization writes a clean, simple loop, times it,
sees a great number, and reports the optimization as a huge win — without noticing the
loop's result was never used. The benchmark isn't lying about the stopwatch reading. It's
measuring something that isn't the thing anyone thinks it's measuring.

## 4. Interview follow-ups

- Purpose-built Java microbenchmark harnesses (JMH being the standard one) have a
  dedicated mechanism for this exact problem — a `Blackhole` object that consumes a
  value in a way the JIT is specifically prevented from optimizing away, precisely
  because hand-rolled defenses like this question's `sink` pattern can, in principle,
  still occasionally be defeated by a sufficiently aggressive future JIT. Why would a
  team building anything beyond a quick, disposable sanity check reach for a real
  harness instead of a hand-rolled loop? Because a hand-rolled benchmark has to get
  warm-up, dead-code elimination, and measurement isolation all correct by hand, every
  time, and a purpose-built harness has already solved each of those problems once,
  correctly, for every benchmark that uses it.
- Does warm-up length matter the same way for every kind of code? No — a method that
  gets inlined and specialized quickly reaches steady state fast; a call site that goes
  through the megamorphic dispatch cliff (question 018) or a deoptimization cycle
  (question 014) may need substantially longer, or a different traffic pattern, to reach
  a stable, representative steady state before a timed region should even start.
