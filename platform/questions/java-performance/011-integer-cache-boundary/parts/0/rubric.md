A good answer covers:

- **Why the identities differ.** `Integer.valueOf(100)` returns the *same* cached object
  every time it's called with 100, because 100 falls inside the `[-128, 127]` cache
  range — both variables hold a reference to the identical object, so `==` (identity)
  is true. `Integer.valueOf(1000)` allocates a fresh object every call because 1000 is
  outside the cache — two separate objects, so `==` is false even though `.equals()`
  would be true for both. A good answer names `Integer.valueOf` as the actual mechanism
  autoboxing calls, not just "Java caches small numbers" as an unexplained fact.
- **The real risk and why it's easy to miss.** Test suites built around small,
  convenient numbers never exercise the allocation path at all — every boxed value in
  tests might be a cache hit, making the code look allocation-free in every test run,
  while production data (real ids, real counts, real timestamps-derived values) is
  overwhelmingly outside the cache range and pays real allocation cost on every box. A
  good answer explicitly names that this isn't a bug that correctness tests can catch —
  the code is correct at every value, it just has a hidden, value-dependent performance
  cliff. A second, related risk worth crediting: code that relies on `==` for boxed
  `Integer` comparison (instead of `.equals()`) can work by accident in tests using small
  numbers and silently break in production once values leave the cache range — a genuine
  correctness bug, not just a performance one, hiding behind the same boundary.
- **What an allocation profile would show.** The production/large-number version would
  show real, attributable allocation weight at every `Integer.valueOf` call site outside
  the cache range — genuine heap objects being created. The small-number test version
  would show this as invisible or near-zero, the same "successfully optimized away"
  shape from question 008, except here the mechanism is a value-based cache hit, not
  escape analysis.

NEEDS_WORK if the answer can't explain why identity comparison differs by value, or
treats this purely as a performance question without noting the `==`-vs-`.equals()`
correctness trap.

## Code

**Incorrect — identity comparison on boxed values, works by accident in tests:**
```java
Integer a = orderId1, b = orderId2;
if (a == b) { ... }             // true only if both happen to be in [-128,127]
```

**Correct — value comparison, correct at every value:**
```java
Integer a = orderId1, b = orderId2;
if (a.equals(b)) { ... }        // or: Objects.equals(a, b) if either could be null
```

**Alternative — avoid boxing entirely if the value is never actually used as an Integer elsewhere:**
```java
int a = orderId1, b = orderId2;
if (a == b) { ... }             // primitive int, no boxing, no cache, no ambiguity
```

