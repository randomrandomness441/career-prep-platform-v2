A good answer covers:

- **What the capture actually does.** `fillInStackTrace()` walks the current thread's
  entire call stack at the moment the exception is constructed, recording every frame
  (class, method, line) into the exception object — real, proportional-to-stack-depth
  work, done unconditionally in the constructor chain of `Throwable`, before anything
  about throwing or catching happens. A good answer names that this happens at
  *construction* time, not at throw time — `new SomeException(...)` alone pays this
  cost even if the exception is never thrown at all.
- **When the override is reasonable, and where the line is.** Reasonable for a genuinely
  internal, tightly-scoped signal that never crosses a boundary where a human or another
  system might need to know *where* it came from — the search-and-immediately-catch
  case is a good fit. Dangerous at a public API boundary or anywhere the exception might
  propagate to a log, a caller, or a debugging session, because removing the stack trace
  removes the only information that would let anyone diagnose an unexpected failure —
  trading a real performance win for a real debuggability loss, appropriate only when
  you're certain nobody will ever need to debug this specific path.
- **Once per instance, not once per throw.** The cost is paid when `fillInStackTrace()`
  runs, which happens during construction (by default, inside `Throwable`'s
  constructor) — reusing a single pre-built exception instance and throwing it
  repeatedly pays the capture cost exactly once, not on each throw. A good answer
  states this distinction plainly: the expensive operation is tied to object
  construction, not to the `throw` keyword.

NEEDS_WORK if the answer thinks the cost is in the `throw`/`catch` mechanism itself, or
recommends removing stack traces from exceptions that can cross a real boundary.

## Code

**Inefficient — a fresh, full-stack-capturing exception for a purely internal signal:**
```java
class FoundException extends RuntimeException {}   // default fillInStackTrace, every throw

Node search(Node root, int target) {
    try {
        walk(root, target);
        return null;
    } catch (FoundException e) {
        return lastFound;
    }
}
void walk(Node n, int target) {
    if (n.value == target) { lastFound = n; throw new FoundException(); }
    for (Node c : n.children) walk(c, target);
}
```

**Correct — override away the capture for a signal that never leaves this method:**
```java
class FoundException extends RuntimeException {
    @Override public synchronized Throwable fillInStackTrace() { return this; }
}
```

**Alternative — reuse a single pre-built instance, paying the (now near-zero) construction cost once:**
```java
private static final FoundException FOUND = new FoundException();   // built once
// ... throw FOUND; anywhere the signal is needed, no per-throw allocation at all
```

