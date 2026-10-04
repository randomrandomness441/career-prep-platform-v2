## 1. Reframe

Deoptimization isn't a bug in the JIT — it's the JIT's safety net for a bet that stopped
paying off. The real question worth asking is never "why did it deoptimize," it's
"was the underlying traffic pattern actually stable, and did something just change it."

## 2. What was actually run, for real

Captured on this machine, JDK 21, `-XX:+UnlockDiagnosticVMOptions -XX:+PrintCompilation
-XX:+TraceDeoptimization`, a loop calling `Shape.area()` on 200,000 `Circle` instances
(warm-up), then 200,000 more alternating `Circle`/`Square`:

```
DeoptDemo::hot (14 bytes)   made not entrant
UNCOMMON TRAP method=DeoptDemo.hot(LShape;)V  bci=4 ... reason=class_check action=maybe_recompile
UNCOMMON TRAP method=DeoptDemo.hot(LShape;)V  bci=4 ... reason=class_check action=maybe_recompile
UNCOMMON TRAP method=DeoptDemo.hot(LShape;)V  bci=4 ... reason=class_check action=maybe_recompile
UNCOMMON TRAP method=DeoptDemo.hot(LShape;)V  bci=4 ... reason=class_check action=maybe_recompile
```

`reason=class_check` is the JIT's speculative type guard failing — exactly the moment
`Square` instances started arriving at a call site the compiler had specialized for
`Circle` alone. `made not entrant` means the specialized compiled version was thrown
away. This repeated multiple times in immediate succession as the mixed traffic began,
then stopped once the JIT had recompiled a version that no longer assumed a single type.

## 3. The broken version, first

Code that's completely correct — both `Circle` and `Square` are valid, correctly
implemented `Shape`s — produces a real, measurable cost cliff purely from *traffic
shape*, not from any logic error. A team debugging a mysterious slowdown right after a
feature flag rollout, looking only at their own new code for a bug, can miss this
entirely if they don't think to check compilation/deoptimization events on the
*existing*, unchanged call site the new type now also flows through.

## 4. Interview follow-ups

- Is a call site that's polymorphic from the very start (never monomorphic, never
  betting wrong) worse off than one that goes through a deopt storm and then stabilizes?
  No — a call site the JIT recognizes as polymorphic early compiles a stable, general
  dispatch strategy from the start, paying a consistent moderate cost with no storm at
  all. The deopt-storm case is specifically the cost of the JIT having bet on the wrong
  pattern initially, not an inherent cost of polymorphism itself.
- Question 008 covered escape analysis being defeated by objects that don't get inlined.
  Is there a connection between inlining and deoptimization? Yes — a method the JIT
  inlined based on an assumption about the callee's concrete type is exactly the kind of
  speculative decision a deoptimization can invalidate; losing an inlining decision to a
  type-check failure is one of the more expensive deopt triggers, since it can unwind
  more compiled code than a simple standalone method's deopt would.
