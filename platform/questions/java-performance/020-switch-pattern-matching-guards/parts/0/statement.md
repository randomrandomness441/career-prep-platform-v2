## ELI5: the directory board that still has to check who you are

Walk into an office building with a big lobby directory: "Floor 3: Circles. Floor 4:
Squares. Floor 5: Triangles." It looks instant — read the sign, go straight to the
floor, no waiting in line. But something still had to figure out which floor to send
you to in the first place, the very first time anyone asked. Once that's settled for
you specifically, going to your floor is instant. The instant-looking sign is doing real
work upstream that isn't visible just from looking at how clean the sign is.

Java's `switch` pattern matching (finalized in JDK 21) has this exact shape.

## What you're actually building (understanding)

Real, verified on this machine, JDK 21 — this compiles and runs exactly as shown:

```java
sealed interface Shape permits Circle, Square, Triangle {}
record Circle(double r) implements Shape {}
record Square(double s) implements Shape {}
record Triangle(double b, double h) implements Shape {}

static double area(Shape shape) {
    return switch (shape) {
        case Circle c when c.r() <= 0 -> 0.0;        // a guarded pattern
        case Circle c -> Math.PI * c.r() * c.r();
        case Square s -> s.s() * s.s();
        case Triangle t -> 0.5 * t.b() * t.h();
    };
}
```

The `when c.r() <= 0` clause is officially called a **guarded pattern** — a boolean
condition attached to a type pattern. This isn't the JIT's internal speculative type
guard from question 014, and it isn't `java.util.regex.Pattern` — it's a real,
first-class Java language feature (JEP 441).

Disassembling the compiled bytecode (`javap -c`) shows something specific: the switch
compiles to `invokedynamic ... typeSwitch:(Ljava/lang/Object;I)I` followed by a real
`tableswitch` — a jump table, not a hand-written chain of `instanceof` checks.

A real, measured timing test with 10 sealed subtypes, repeatedly dispatching to either
the *first*-declared case or the *last*-declared case, 300 million calls each after
warm-up:

```
dispatch to FIRST-listed case:  286.8ms
dispatch to LAST-listed case:   326.7ms
```

And a separate, real compiler-behavior test: writing two guarded cases with the exact
same condition, where the second is provably, permanently unreachable —

```java
case Circle c when c.r() > 0 -> "positive circle";
case Circle c when c.r() > 0 -> "UNREACHABLE -- never hit";   // identical guard
```

— compiles clean, zero errors, zero warnings, even with `-Xlint:all` enabled.

## Requirements

1. Given the real bytecode (`invokedynamic:typeSwitch` feeding a `tableswitch`), is
   `switch` pattern matching on a sealed hierarchy architecturally the same thing as a
   hand-written `if (x instanceof Circle c) ... else if (x instanceof Square s) ...`
   chain, or something different? Name the concrete difference visible in the bytecode.
2. The FIRST-vs-LAST timing gap (286.8ms vs 326.7ms) is real, but modest — about 14%,
   not the much larger gap you'd expect if the switch were secretly checking each of the
   10 types one by one, in order, on every single call. What does this measured
   *magnitude* actually tell you about the dispatch mechanism, without overclaiming a
   specific internal implementation you haven't directly verified?
3. Why does the compiler catch an unreachable case for a plain, unguarded type pattern
   (listing a supertype case before a subtype case is a compile error) but not for two
   guarded patterns with an identical, always-true-together condition? What does this
   tell you about what the compiler can and can't reason about at compile time?

## Why this matters

A new, cleaner-looking syntax feature is not automatically a new, free performance
mechanism, and it's not automatically exempt from the same safety analysis older syntax
gets — both need to be checked on their own terms, not assumed from how nice the syntax
looks.
