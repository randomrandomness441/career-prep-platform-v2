## 1. Reframe

"Polymorphism has a cost" is usually taught as a vague, smooth tax. The real, measured
shape is a hard design limit: exactly two cached types are nearly free, a third is a
cliff, and the cliff is reversible if the pattern later narrows back down.

## 2. What was actually measured, for real

Four separate real benchmarks, JDK 21, 200 million calls each after full warm-up on a
fixed type distribution:

```
monomorphic (1 type):   253.5ms
bimorphic   (2 types):  247.0ms
trimorphic  (3 types):  844.3ms
megamorphic (4 types):  765.0ms
```

And a separate recovery test, same process, no restart:

```
mono before:      249.7ms
went trimorphic:  651.1ms
mono again:       270.5ms  (close to original, not stuck at the trimorphic cost)
```

## 3. The broken version, first

The natural mental model most engineers carry is "polymorphism costs a bit more per
extra type, roughly proportionally" — a smooth curve. The measured reality is a step
function: nothing meaningfully changes between 1 and 2 types, then a real ~3x jump at
exactly 3, then no further meaningful change from 3 to 4. Reasoning about polymorphism
cost with a smooth-curve mental model gets the shape of the actual cost wrong, not just
the magnitude.

## 4. Interview follow-ups

- Question 019 covered a logging library design that swaps guard objects only when
  configuration changes, keeping the actual log call sites monomorphic or bimorphic.
  Why does this question's finding make that design choice specifically effective,
  beyond just "avoiding an if check"? Because the technique's win depends on the call
  site staying within the two-type fast-path budget — if the logging framework
  routinely swapped between three or more distinct guard implementations at the same
  call site, it would push that site into the same megamorphic cost tier this question
  measured, undermining the whole point of the design.
- Does this same two-type cliff apply to method calls that aren't through an interface
  — a `static` method, or a `final` class's method? No — the inline cache and its
  monomorphic/bimorphic/megamorphic tiers exist specifically to handle virtual dispatch,
  where the JIT doesn't statically know which concrete implementation will run. A
  `static` call or a call on a `final` class has exactly one possible target already
  known at compile time — there's no polymorphism to speculate about, so this entire
  cost structure doesn't apply to it at all.
