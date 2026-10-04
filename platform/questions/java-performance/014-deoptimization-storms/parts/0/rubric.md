A good answer covers:

- **What the JIT bet on, and why it's reasonable.** It bet that `hot` would keep seeing
  only `Circle` at that call site, based on everything it observed during warm-up — a
  monomorphic call site (always the same concrete type) can be compiled with a direct,
  inlined call instead of a virtual dispatch, which is significantly faster. Betting on
  the pattern continuing is reasonable specifically *because* profiling never saw
  anything else — the JIT has no way to know the future traffic mix, only what it's
  observed so far, and optimizing for the observed-so-far pattern is the entire point of
  profile-guided speculative optimization.
- **The JIT stops betting wrong eventually.** After enough repeated deoptimizations at
  the same call site for the same reason, HotSpot gives up trying to re-specialize and
  compiles a **megamorphic** version instead — using general virtual dispatch rather
  than a speculative inline-cache guess. This version is slower per call than a
  successful monomorphic inline cache, but stable: it doesn't need to deoptimize again
  when a third or fourth `Shape` type shows up, because it was never betting on a
  specific type in the first place. A good answer names this progression
  (monomorphic-guess → repeated deopt → megamorphic/stable) explicitly, not just "it
  eventually stops."
- **A real-world trigger.** A feature flag or canary rollout that introduces a second
  implementation into a call site that had only ever seen one concrete type since the
  service started warm — the exact shape of "mostly monomorphic, then suddenly
  polymorphic, with no code change at the call site itself." A plugin/handler registry
  gaining a new registered type after the service has been running is an equally valid
  answer.

NEEDS_WORK if the answer thinks the JIT keeps deoptimizing forever with no stable
end state, or can't explain why the initial monomorphic bet was reasonable given only
what the JIT had observed.

## Code

There's no "buggy" code here — polymorphism itself isn't wrong. The lever is call-site
shape, not correctness.

**One shared call site sees every type, forcing it megamorphic:**
```java
double totalArea(List<Shape> shapes) {
    double sum = 0;
    for (Shape s : shapes) sum += s.area();   // one call site, mixes every Shape type
    return sum;
}
```

**Alternative — split by type at a hot call site, so each one stays mono/bimorphic:**
```java
double totalArea(List<Shape> shapes) {
    double sum = 0;
    for (Shape s : shapes) {
        if (s instanceof Circle c) sum += c.area();        // its own call site
        else if (s instanceof Square sq) sum += sq.area();  // its own call site
        else sum += s.area();                                // fallback, rarer types
    }
    return sum;
}
```

Worth doing only once profiling shows this specific call site is both hot and
genuinely megamorphic — splitting adds real code complexity for a win that doesn't
matter on a cold or already-stable path.

