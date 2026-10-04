A good answer covers:

- **Why the cost is worse than linear.** Each `s = s + ...` copies the *entire current
  contents* of `s` into a new string, plus the new piece. As `s` grows longer, each
  individual concatenation costs more (proportional to the current length), and this
  happens on every iteration — the total cost across `n` iterations sums roughly to
  1+2+3+...+n, which is O(n²), not O(n). A good answer explicitly names the growing
  per-iteration cost, not just "it's slow because strings are immutable" without
  connecting immutability to the actual quadratic shape.
- **Why the single-expression case is genuinely fine.** The compiler can see the entire
  concatenation expression at once and generate one efficient combining operation for
  all the pieces together (via `invokedynamic` and `StringConcatFactory` on modern Java),
  rather than a sequence of separate rebuild-the-whole-string-so-far operations. There's
  no *growing* intermediate string being repeatedly rebuilt — every piece is known and
  combined once. A good answer names that the problem is specifically about *repeated*
  concatenation onto an already-built, growing result, not about the `+` operator
  itself.
- **Yes, shrinking has the same cost, and `StringBuilder` still helps.** `String`
  hasn't shared backing character storage between a string and its substrings since
  Java 7 update 6 — every `substring()` call builds a new character array copy, so
  repeatedly shrinking via `substring` pays the same "copy nearly everything, every
  call" cost as repeatedly growing via `+`. `StringBuilder` (via `setLength()` or
  `deleteCharAt()`) can shrink in place without needing to reallocate and copy a whole
  new backing array the way immutable `String` operations do, so it still helps here.

NEEDS_WORK if the answer thinks the single-expression case is also a performance risk,
or thinks shrinking a string via `substring` avoids the copying cost that growing has.

## Code

**Inefficient — rebuilds the entire string on every iteration:**
```java
String s = "";
for (int i = 0; i < n; i++) {
    s = s + i % 10;   // full copy of everything built so far, every time
}
```

**Correct — StringBuilder grows its backing buffer in place:**
```java
StringBuilder sb = new StringBuilder();
for (int i = 0; i < n; i++) {
    sb.append(i % 10);
}
String s = sb.toString();
```

**Fine as-is — a single concatenation expression, no loop, compiler combines it in one pass:**
```java
String msg = "user " + id + " did " + action + " at " + time;   // one invokedynamic call
```

