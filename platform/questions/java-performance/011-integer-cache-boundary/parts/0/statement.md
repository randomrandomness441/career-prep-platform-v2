## ELI5: the pre-made sandwiches vs. made-to-order

A deli keeps a tray of the 256 most commonly ordered sandwiches pre-made and ready to
hand over instantly — no cooking, no cost, just grab and go. Order something off that
tray and it's free and instant. Order anything else, and the deli makes it fresh, from
scratch, every single time you ask, even if you ask for the exact same off-tray
sandwich a thousand times in a row.

Java's `Integer` autoboxing works exactly like this. `Integer.valueOf(int)` — which is
what autoboxing calls under the hood — keeps a cache of pre-made `Integer` objects for
values from **-128 to 127**. Box anything in that range and you get a shared, reused
object for free. Box anything outside it and a brand new object gets allocated, every
single time, even for the same value repeated a million times.

## What you're actually building (understanding)

Real, measured output on this machine, JDK 21:

```
100 == 100 (boxed):   true
1000 == 1000 (boxed): false
```

Two `Integer` variables both holding the value 1000, compared with `==` (reference
identity, not `.equals()`), come back `false` — they're two different objects. The same
comparison at 100 comes back `true` — same cached object, reused.

Timing a tight loop boxing 50,000,000 values, on the same machine:

```
cached-range boxing (always 100):        7.4ms
outside-range boxing (100 + i%1000):    42.5ms
```

## Requirements

1. Why does `100 == 100` come back `true` for boxed `Integer`s when `1000 == 1000`
   doesn't, given that `==` on object references checks identity, not value equality?
2. A team's test suite uses small, easy-to-read numbers (ids 1-20, counts 0-10) and
   never shows any allocation pressure from boxing. Production data routinely produces
   values in the thousands. What's the real risk here, and why would this specific gap
   between test and production behavior be easy to miss in code review?
3. If you profiled the production version with an allocation flame graph (from question
   008's toolkit), what would you expect to see that wouldn't show up at all when
   profiling the test suite's small-number version?

## Why this matters

This is a case where the exact same source code has genuinely different real
performance characteristics depending purely on the runtime *values* flowing through
it, not the code's structure — invisible in a code review, invisible in a test suite
built around convenient small numbers, and only visible once you either know the
boundary exists or profile real production data.
