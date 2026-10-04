## ELI5: rewriting the whole letter to add one more word

A `String` in Java is immutable — once created, it never changes. `s = s + "x"` doesn't
add `"x"` onto the end of the existing string; it builds a brand new string containing
everything the old one had, plus `"x"`, and points `s` at the new one. It's like adding
one word to a letter by copying the entire letter over again from scratch, word for
word, just to add that one word at the end — and doing that again for the next word, and
the next.

## What you're actually building (understanding)

Real, measured on this machine, JDK 21 — building a string of 60,000 characters, one
character at a time:

```java
String s = "";
for (int i = 0; i < n; i++) s = s + i % 10;
```
```
String += in a loop (n=60000):    128ms
```

versus

```java
StringBuilder sb = new StringBuilder();
for (int i = 0; i < n; i++) sb.append(i % 10);
String s = sb.toString();
```
```
StringBuilder.append (n=60000):     2ms
```

About **64x**. `StringBuilder` is mutable — `append` grows an internal buffer in place
(occasionally reallocating and copying, the same amortized-growth idea as
question 027's `ArrayList`), never rebuilding everything that came before.

## Requirements

1. Why does the cost of `s = s + i % 10` inside a loop grow *worse* than linearly as the
   string gets longer, rather than costing the same fixed amount on every iteration?
2. A single line like `String msg = "user " + id + " did " + action + " at " + time;`
   (no loop, one concatenation expression) is *not* a performance problem, even though
   it also uses `+`. What's actually different between this and the loop case, given
   that both use string concatenation?
3. Would wrapping the loop body in `StringBuilder` help if, instead of appending small
   pieces, the loop repeatedly called `s.substring(0, s.length() - 1)` to *remove* the
   last character each time? Reason about whether the same immutability cost applies to
   shrinking a string, not just growing one.

## Why this matters

This is one of the oldest, most-taught Java performance lessons, and it's still worth
knowing *why* it's true rather than just memorizing "use StringBuilder in loops" — the
mechanism (immutability meaning every change is a full rebuild) explains both why the
loop case is bad and why the single-expression case genuinely isn't.
