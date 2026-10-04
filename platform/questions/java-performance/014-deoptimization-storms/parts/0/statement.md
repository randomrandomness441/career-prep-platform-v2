## ELI5: the shortcut that only works if nothing unexpected happens

A delivery driver learns a route so well they stop checking street signs entirely —
they've driven it a thousand times, it's always the same, checking signs is now wasted
effort. Then, one day, a detour sign appears. The driver, running on pure memorized
habit, has to stop, actually look around, re-figure-out where they are, and drive more
carefully for a while until they've learned the new pattern (or the detour goes away
and they go back to autopilot). That moment of "wait, stop, look around again" is real
lost time, paid specifically because the shortcut had stopped being valid.

The JIT does the same thing. Once it's seen a call site behave one way (say, always
calling the same concrete class through an interface) enough times, it speculatively
compiles a fast, specialized version that assumes that pattern holds. If the pattern
breaks — a new type shows up — the JIT can't trust its fast version anymore and
**deoptimizes**: throws away the specialized code and drops back to slower, safer
execution (the interpreter or a less-optimized compile) for that call site, at least
temporarily.

## What you're actually building (understanding)

Real output, captured on this machine with `-XX:+PrintCompilation -XX:+TraceDeoptimization`,
from a loop that calls `Shape.area()` through 200,000 `Circle` instances (JIT compiles
a fast, `Circle`-specialized version), then immediately switches to a mix of `Circle`
and `Square` instances for another 200,000 calls:

```
DeoptDemo::hot (14 bytes)   made not entrant
UNCOMMON TRAP method=DeoptDemo.hot(LShape;)V  bci=4 ... reason=class_check action=maybe_recompile
UNCOMMON TRAP method=DeoptDemo.hot(LShape;)V  bci=4 ... reason=class_check action=maybe_recompile
UNCOMMON TRAP method=DeoptDemo.hot(LShape;)V  bci=4 ... reason=class_check action=maybe_recompile
```

The moment `Square` instances start arriving, the specialized compiled version of `hot`
gets marked "not entrant" (thrown away) and the JIT repeatedly bails out to the
interpreter (`reason=class_check`) until it can recompile a version that handles both
types.

## Requirements

1. Why did the JIT specialize `hot` for `Circle` specifically in the first place —
   what was it betting on, and why is that a reasonable bet to make from a profiler's
   point of view during warm-up?
2. If a service's real traffic mix genuinely, permanently includes many different
   `Shape` implementations from the start (not a sudden late arrival, just always
   polymorphic), would you expect the same repeated deoptimization storm to keep
   happening indefinitely, or does the JIT eventually stop betting wrong? What does the
   JIT do differently once it has seen enough real diversity at a call site?
3. This exact scenario — code that's mostly monomorphic, then suddenly becomes
   polymorphic — describes a specific, common real-world situation in long-running
   services. Name one.

## Why this matters

This bug doesn't come from anything wrong in the code — `Circle` and `Square` are both
completely valid `Shape` implementations, used correctly. The cost is a pure side
effect of the JIT's own optimization strategy meeting a traffic pattern it didn't
expect, invisible unless you know to look at compilation/deoptimization events
specifically.
