A good answer covers:

- **Why random costs more than the same amount of predictable work.** The CPU can't
  know the future either way, but a predictable pattern lets its branch predictor guess
  correctly almost every time based on recent history, making each correct guess
  essentially free. A truly random 50/50 pattern defeats prediction entirely — roughly
  half of all guesses are wrong, and each wrong guess means discarding speculatively
  executed work and restarting down the correct path, a real, repeated cost paid on top
  of whatever the branch's outcome actually was. The *amount* of real work done isn't
  what differs between the two patterns — the *predictability* is.
- **The real explanation for the 500x number, not just "virtual calls beat branches."**
  A consistently, perfectly predicted `if (isEnabled())` (always false, every call)
  should already be nearly free for the branch predictor on its own — that alone can't
  explain a 500x gap. The dominant real mechanism is almost certainly that a naive
  `if (isEnabled()) { log("x=" + expensiveToString(x)); }` still evaluates its
  arguments (string concatenation, `expensiveToString`) *before* the check ever runs in
  many real call patterns, while a guard object that routes disabled calls to a literal
  no-op short-circuits that evaluation entirely, skipping real work rather than just
  avoiding a mispredicted branch. A good answer names eager argument evaluation as the
  main driver here, not branch prediction — crediting *only* branch-prediction avoidance
  for a gap this large is the wrong mechanism, even though branch prediction is real and
  relevant to the smaller mixed-levels numbers in the same benchmark.
- **Why it wouldn't help if the guard itself flips unpredictably per call.** Question
  018 showed the fast dispatch path (mono/bimorphic) breaks down specifically once a
  call site sees more than two distinct concrete types — but here, swapping which guard
  object is installed on every single call would make that call site itself
  polymorphic and unstable in a *different* way: constant reconfiguration defeats the
  whole premise of "decided rarely, dispatched often" that makes the technique work in
  the logging case. If the enabled/disabled decision is genuinely unpredictable per
  call, no architecture eliminates the need to make that decision per call — the
  technique's win specifically depends on the guard changing rarely, not on every call.

NEEDS_WORK if the answer attributes the full 500x figure to branch-prediction avoidance
alone, or thinks the guard-object technique works regardless of how often the guard's
value actually changes.

## Code

**Inefficient — arguments built eagerly, before the check ever runs:**
```java
if (logger.isDebugEnabled()) {
    logger.debug("x=" + expensiveToString(x));   // still eager if this line is reached...
}
// worse, common variant: the concatenation happens unconditionally
logger.debug("x=" + expensiveToString(x));       // built every call, level or not
```

**Correct — a guard object routes disabled calls to a real no-op, argument never built:**
```java
sealed interface LevelGuard permits DebugGuard, NoopGuard {
    LoggingEventBuilder debug();
}
record NoopGuard() implements LevelGuard {
    public LoggingEventBuilder debug() { return NOPLoggingEventBuilder.INSTANCE; }
}
// swapped only when configuration changes, not per call
guard.debug().log("x={}", () -> expensiveToString(x));   // supplier, never invoked if disabled
```

**Alternative — supplier-based lazy logging, no architectural change, still avoids eager evaluation:**
```java
logger.atDebug().log("x={}", () -> expensiveToString(x));   // lambda only runs if enabled
```

