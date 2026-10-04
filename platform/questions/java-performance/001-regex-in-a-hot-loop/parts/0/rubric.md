A good answer covers:

- **Constant-per-call, not quadratic.** Recompiling costs roughly the same fixed amount
  every call regardless of how many calls came before it — it doesn't grow with how much
  work has already happened, unlike `dedupe`'s O(n^2) scan. That's why the gap here is a
  flat multiplier (~7x) instead of exploding at scale the way question 009's bug did.
  Both are real, both are avoidable, but they're different shapes of cost, and a good
  answer distinguishes "wasteful but linear" from "wasteful and quadratic."
- **What a flame graph would show.** A wide frame inside `java.util.regex.Pattern.compile`
  (and the parsing machinery underneath it), showing up under every call site that calls
  `.matches()` — not obviously connected to "my business logic," which is exactly why
  people miss it reading code: the expensive part is a standard library call that looks
  innocuous.
- **Thread-safe reuse.** Compile the `Pattern` once (a `static final` field, for instance),
  and create a new `Matcher` per call/thread via `pattern.matcher(input)` — the `Matcher`
  itself is cheap to create and holds the mutable matching state, so each thread gets its
  own `Matcher` from the one shared, immutable, thread-safe `Pattern`.

NEEDS_WORK if the answer calls this the same shape of bug as a quadratic algorithm, or
proposes sharing a single `Matcher` instance across threads.

## Code

**Inefficient — recompiles the regex on every call:**
```java
boolean isValid(String s) {
    return s.matches("user-\\d+-active");   // compiles a new Pattern every call
}
```

**Correct — compile once, reuse the compiled Pattern:**
```java
private static final Pattern USER_ACTIVE = Pattern.compile("user-\\d+-active");

boolean isValid(String s) {
    return USER_ACTIVE.matcher(s).matches();   // Pattern is shared, Matcher is cheap
}
```

**Alternative — if the check is genuinely just a prefix/suffix test, skip regex entirely:**
```java
boolean isValid(String s) {
    return s.startsWith("user-") && s.endsWith("-active");   // no backtracking engine at all
}
```

