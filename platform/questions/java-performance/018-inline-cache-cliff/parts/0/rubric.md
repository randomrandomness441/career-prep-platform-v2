A good answer covers:

- **Why the cliff is exactly at 2→3, not gradual.** HotSpot's inline cache for a call
  site caches up to two concrete receiver types, each checked with a fast, direct
  comparison — that's why 1 and 2 types cost almost the same (measured: 253.5ms vs
  247.0ms). The moment a third distinct type appears, the two-slot cache can't
  represent it, and the JIT abandons the fast comparison-based approach entirely for a
  different, more general dispatch mechanism for every future call at that site —
  explaining why 3 types (844.3ms) already costs as much as 4 (765.0ms): the cliff is a
  design limit of exactly two cached types, not a smooth cost curve.
- **Whether it recovers, and the surprising real answer.** Most people's first guess is
  "no, permanent for the process's life" — that guess is wrong. Measured on this
  machine: monomorphic (249.7ms) → trimorphic (651.1ms) → back to monomorphic-only
  traffic (270.5ms), all in the same running process, no restart. The call site *did*
  recover close to its original fast-path cost once the type mix narrowed back down. A
  good answer credits this as a real, measured recovery, not a permanent one-way door —
  the mechanism is the JIT's ongoing profile-guided recompilation (question 014):
  continued profiling can trigger a fresh compile with updated type information,
  producing a new, re-specialized inline cache once the observed pattern has genuinely
  changed again.
- **Why 15 total implementations doesn't matter on its own.** The cliff is about how
  many *distinct concrete types actually arrive at one specific call site*, not how many
  implementations exist anywhere in the codebase. Two call sites, each seeing only 1-2
  of the 15 types in practice, each get their own independent inline cache and stay
  fast — the total size of a type hierarchy is irrelevant to this cost; what matters is
  the local, per-call-site type diversity actually observed at runtime.

NEEDS_WORK if the answer assumes cost scales gradually with type count, assumes a
megamorphic call site can never recover, or thinks total implementation count (rather
than per-call-site diversity) drives this cost.

## Code

**One call site absorbing every concrete type — pushes past the two-type cache:**
```java
double totalArea(List<Shape> shapes) {   // 15 possible Shape implementations
    double sum = 0;
    for (Shape s : shapes) sum += s.area();   // one call site sees all 15
    return sum;
}
```

**Alternative — route by type up front, so each call site stays within budget:**
```java
double totalArea(List<Circle> circles, List<Square> squares) {   // separate collections
    double sum = 0;
    for (Circle c : circles) sum += c.area();   // its own call site, monomorphic
    for (Square s : squares) sum += s.area();   // its own call site, monomorphic
    return sum;
}
```

Only worth this restructuring once profiling (question 018's own methodology) shows a
specific call site is both hot and genuinely past two types in practice — most real
call sites never come close.

