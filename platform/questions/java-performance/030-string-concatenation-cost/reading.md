## 1. Reframe

Immutability isn't free. Every "modification" to a `String` is actually a brand new
object, and the cost of building that new object scales with how much content it has
to copy from the old one — invisible from reading a single `+`, very visible once it
happens in a loop.

## 3. The broken version, first

`s += piece` inside a loop reads as completely ordinary code — it compiles, it's
correct, and for a short loop it's genuinely fine. The cost only becomes real once the
loop runs enough times on a growing string that the repeated full-copy cost adds up —
exactly the kind of bug that passes every small test and only shows up at real scale,
the same shape as several other findings in this pack.

## 4. Interview follow-ups

- Does `String.join(delimiter, list)` or `Stream.collect(Collectors.joining())` avoid
  this cost for combining many strings? Yes — both are implemented to build the result
  efficiently in one pass (internally using something like a `StringBuilder` or
  equivalent), not via repeated `+=`-style concatenation, so they don't pay the
  quadratic cost this question measured.
- `StringBuilder` isn't thread-safe; `StringBuffer` is, via synchronized methods. Given
  question 019's finding that even a perfectly-predicted `if` check is nearly free, is
  `StringBuffer`'s synchronization overhead worth avoiding by default? For the common
  case of a `StringBuilder` used entirely within one thread (the overwhelming majority
  of string-building code), yes — `StringBuffer`'s synchronization is real, unnecessary
  overhead paid on every single append when nothing is actually contending for it, which
  is exactly why `StringBuilder` was introduced as the default choice and `StringBuffer`
  is reserved for the specific, rarer case of a builder genuinely shared across threads.
