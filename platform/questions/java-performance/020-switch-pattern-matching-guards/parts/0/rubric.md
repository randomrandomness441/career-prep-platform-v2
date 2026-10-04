A good answer covers:

- **Architecturally different, concretely.** A hand-written `if/else instanceof` chain
  compiles to a literal sequence of type checks in bytecode, one per branch, evaluated
  in source order every time. The pattern-matching switch instead resolves *which* case
  applies via a single `invokedynamic` call to a `typeSwitch` bootstrap, which returns an
  integer index, and *that* index feeds a real `tableswitch` — a genuine O(1) jump once
  the index is known. The concrete difference: the dispatch mechanism after resolution
  is structurally different (jump table vs. sequential branches), even though something
  still has to do the work of resolving the index in the first place.
- **What the modest 14% gap actually shows, without overclaiming.** It rules out the
  simplest "still fully sequential, checked one-by-one every call" model — a true O(n)
  linear scan across 10 types would produce a far larger gap between position 1 and
  position 10 than 14%. It does NOT prove the mechanism is fully free of
  position-dependence either — the gap is real and repeatable, not noise. A good answer
  states both halves: rules out the worst-case naive model, but doesn't claim to have
  fully explained or verified the exact underlying implementation from this measurement
  alone. NEEDS_WORK if the answer treats the 14% gap as proof of a specific mechanism it
  hasn't actually confirmed, in either direction (fully sequential, or fully cached and
  position-independent).
- **What "guarded" cases losing dominance checking reveals.** The compiler can reason
  about type relationships statically (a supertype pattern before a subtype pattern is
  always fully covered, checkably, from the type hierarchy alone) — that's decidable at
  compile time. A guard is an arbitrary boolean expression, and proving two arbitrary
  boolean expressions are related (identical, overlapping, mutually exclusive) is not
  something the compiler attempts in general — it would require evaluating expressions,
  not just types. A good answer names this distinction: pure type-pattern dominance is
  structural and checkable; guard-condition overlap is a general boolean satisfiability
  problem the compiler doesn't attempt to solve, so it simply doesn't check guarded
  cases for dominance at all, silently accepting even provably dead ones.

NEEDS_WORK if the answer thinks pattern-matching switch is just syntax sugar for an
instanceof chain with no bytecode-level difference, or can't explain why guards
specifically defeat the compiler's dominance analysis.

## Code

**Older style — a hand-written, sequential instanceof chain:**
```java
double area(Shape shape) {
    if (shape instanceof Circle c) {
        return c.r() <= 0 ? 0.0 : Math.PI * c.r() * c.r();
    } else if (shape instanceof Square s) {
        return s.s() * s.s();
    } else if (shape instanceof Triangle t) {
        return 0.5 * t.b() * t.h();
    }
    throw new IllegalStateException();
}
```

**Correct — pattern-matching switch with a guarded pattern, compiles to invokedynamic + tableswitch:**
```java
double area(Shape shape) {
    return switch (shape) {
        case Circle c when c.r() <= 0 -> 0.0;   // guarded pattern
        case Circle c -> Math.PI * c.r() * c.r();
        case Square s -> s.s() * s.s();
        case Triangle t -> 0.5 * t.b() * t.h();
    };
}
```

**Silent gap worth knowing — duplicate guarded cases compile clean, even when provably dead:**
```java
case Circle c when c.r() > 0 -> "positive circle";
case Circle c when c.r() > 0 -> "UNREACHABLE";   // identical guard -- no compiler warning
```

