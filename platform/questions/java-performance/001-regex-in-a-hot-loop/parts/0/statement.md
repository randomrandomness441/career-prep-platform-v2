## ELI5: rebuilding the stamp before every use

Imagine a mail room that stamps every envelope with the same rubber stamp. A lazy
worker throws the stamp away after every single envelope, then carves a brand new one
from scratch before stamping the next one. It still works. Every envelope gets stamped
correctly. It's also absurd, because carving the stamp is far more work than actually
using it, and nothing about the stamp changed between envelopes.

`String.matches(regex)` does exactly this every time you call it: it compiles the regex
into a `Pattern` from scratch, uses it once, and throws it away. Call it in a loop and
you're carving a new stamp on every iteration.

## What you're actually building (understanding)

Two real, measured runs on this machine, 200,000 strings, same regex, same input:

```
recompile-each-call: 200000 matches, 98.1ms
compiled-once:       200000 matches, 13.4ms
```

The only difference between the two loops: one calls `s.matches("user-\\d+-active")`
directly (recompiles every call), the other compiles the `Pattern` once outside the
loop and reuses it with `.matcher(s).matches()`.

## Requirements

1. Why is the gap here (~7x) smaller than the gap you'd expect from a truly quadratic
   bug like question 009's `dedupe`? What's fundamentally different about this cost
   compared to that one — is this cost proportional to input size squared, or just a
   constant per-call tax that happens to add up?
2. If you profiled the slow version with a CPU flame graph, what would you expect to
   see as a wide, unexpected frame that has nothing to do with your own business logic?
3. `Matcher` objects are not thread-safe, but a compiled `Pattern` is. In a
   multi-threaded service handling many requests concurrently, what's the right way to
   reuse the compiled pattern without threads corrupting each other's matching state?

## Why this matters

This is the most common Java performance mistake there is, precisely because it's
invisible without profiling: the code is correct, the regex is only one line, and
nothing about `s.matches(...)` looks expensive to someone reading it casually.
