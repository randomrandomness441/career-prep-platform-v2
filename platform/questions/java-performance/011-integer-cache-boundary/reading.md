## 1. Reframe

Autoboxing looks like one uniform operation from the source code. It is not — its real
cost depends on the runtime value being boxed, not on anything visible in the code
itself. That's a genuinely unusual shape of performance bug: identical code, different
cost, based purely on data.

## 3. The broken version, first

A team writes and tests code using small ids (1-20) throughout development. Everything
passes, nothing looks slow, allocation profiling (if anyone even runs it) shows nothing
interesting because every boxed value is a cache hit. In production, real user or order
ids in the thousands or millions flow through the exact same code path, and every one
of them allocates. Nobody changed a line of code between test and production — the
values did all the work of exposing or hiding the cost.

## 4. Interview follow-ups

- Is the `[-128, 127]` range fixed forever, or can it change? It's guaranteed to include
  at least `-128` to `127` by the JLS, but the upper bound is configurable via
  `-XX:AutoBoxCacheMax` on HotSpot — a service that's found this boundary matters in
  practice could widen the cache rather than changing code, trading a small amount of
  fixed memory for fewer allocations, though that only helps if the real value
  distribution clusters just outside the default range.
- Does this same caching trick apply to other boxed primitive types? Yes —
  `Boolean.valueOf`, `Byte.valueOf`, `Short.valueOf`, `Long.valueOf` (same -128 to 127
  range), and `Character.valueOf` (0 to 127) all cache in the same way; `Integer` is
  just the one that comes up most because ids and counts are the most common
  boxed-and-compared values in real code.
