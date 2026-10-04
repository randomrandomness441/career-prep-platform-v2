## 1. Reframe

"Pattern matching" in Java means a specific, real language feature (JEP 441, finalized
JDK 21) — type patterns and guarded patterns in `switch` — not `java.util.regex.Pattern`
(a library class for text matching) and not the JIT's internal speculative type guards
(an implementation detail of how HotSpot compiles polymorphic calls). All three are real
and all three are genuinely different things that happen to share overlapping
vocabulary.

## 2. What was actually verified, for real

Three separate things confirmed on this machine, JDK 21, not asserted from memory:

- A guarded-pattern switch (`case Circle c when c.r() <= 0 -> 0.0`) compiles and runs
  correctly, guard included.
- `javap -c` on the compiled class shows `invokedynamic #13,0 // typeSwitch:(Ljava/lang/Object;I)I`
  feeding directly into a `tableswitch` — a real, distinct dispatch mechanism from a
  hand-written `instanceof` chain, visible in the actual bytecode, not inferred.
- Two identically-guarded `case Circle c when c.r() > 0` branches, the second one
  permanently unreachable, compile with zero errors and zero warnings — even with
  `-Xlint:all` — confirming the documented behavior that guarded cases are excluded
  from the compiler's dominance/exhaustiveness analysis.

## 3. The broken version, first

The natural assumption when a new syntax feature ships is either "it's just sugar, same
cost as what I'd have written by hand" or "the compiler surely still catches obvious
mistakes here the way it does everywhere else." Both assumptions turned out to need
checking rather than trusting on this feature specifically: the bytecode is genuinely
different from a hand-written chain, and the compiler genuinely does not catch an
obviously dead guarded case that it would catch for an equivalent unguarded one.

## 4. Interview follow-ups

- If dominance checking doesn't cover guards, is there any compile-time safety net left
  for a `switch` on a sealed type at all? Yes — exhaustiveness over the *type* dimension
  still works: the compiler still requires every permitted subtype to be covered by at
  least one case (with or without a guard), and a `switch` missing a subtype entirely is
  still a compile error. What's lost is only the finer-grained reasoning about whether a
  specific *guard condition* makes a case reachable — the type-level safety net stays
  intact.
- Question 018 found dispatch through an interface reference gets dramatically more
  expensive past two concrete types (the megamorphic cliff). Does the `typeSwitch`
  bootstrap for a `switch` on a sealed hierarchy risk the same cliff if the hierarchy is
  large and varied at one call site? Plausibly related, though this question didn't
  directly test that specific comparison — `typeSwitch`'s bootstrap is itself backed by
  a real `invokedynamic` call site, and call sites in general are exactly the kind of
  thing question 018's inline-cache mechanics apply to. Worth measuring directly before
  asserting it definitely does or doesn't share that exact cliff, rather than assuming
  either way from architecture alone.
