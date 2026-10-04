A good answer covers:

- **What's wrong with "always a risk, that's just a fact."** It's an overgeneralization
  from older engine behavior (and other languages' engines, some of which genuinely
  still have no such protection) applied uncritically to modern `java.util.regex` without
  checking. The measured data directly contradicts "always" for these three classic
  patterns on this JDK. A good answer doesn't just say "that's wrong" — it points at the
  specific measured evidence that contradicts the blanket claim.
- **What's wrong with "OpenJDK fixed it, don't think about it."** The optimization is
  explicitly documented as partial mitigation, not a proof that every possible
  catastrophic construction is now safe — treating a specific, scoped engineering fix as
  a permanent, universal guarantee is exactly the same kind of unverified leap as the
  first colleague's claim, just pointed in the opposite direction. A good answer names
  that "partial" specifically means there's no claim of completeness, not that the risk
  is merely reduced everywhere.
- **Reasoning about the fix's actual limits.** The memoization keys on input cursor
  position alone, assuming that failing at a given position means it will always fail
  there again. Anything that breaks that assumption — most plausibly, **backreferences**
  (where whether a later part of the pattern matches depends on what an earlier group
  actually captured, not just where the cursor is) — could mean the "same cursor
  position" isn't actually the same subproblem twice, since the captured state differs
  across different backtracking paths even at an identical position. Lookahead/lookbehind
  combined with repetition, where the assertion's result depends on more than position,
  is an equally valid answer. Full credit doesn't require a personally verified example —
  this question doesn't have one on file either — it requires sound reasoning from what
  cursor-position-only memoization does and doesn't capture.

NEEDS_WORK if the answer treats either colleague's claim as simply correct, or reasons
about requirement 3 without engaging with what the memoization key (cursor position)
actually is.

## Code

**Plausibly still risky — a repetition whose outcome depends on more than cursor position:**
```java
// backreference: whether the tail matches depends on what group 1 captured,
// not just where the cursor is -- may defeat cursor-only memoization
Pattern.compile("^(\\w+)\\s+\\1+$");
```

**Safer alternative — avoid backreferences/nested quantifiers when a simpler pattern says the same thing:**
```java
// no backreference, no nested repetition ambiguity
Pattern.compile("^(\\w+)(\\s+\\w+)*$");
```

**Defense-in-depth for untrusted regex, independent of the engine's own mitigations:**
```java
Future<Boolean> result = executor.submit(() -> pattern.matcher(input).matches());
result.get(200, TimeUnit.MILLISECONDS);   // bounded budget, regardless of pattern shape
```

