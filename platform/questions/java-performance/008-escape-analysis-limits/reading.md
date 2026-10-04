## 1. Reframe

"The JIT handles that" is a claim about a specific optimization succeeding, not a law
of nature. Escape analysis is real and powerful, and it has real, documented limits —
the only way to know which side of those limits your code is on is to measure.

## 3. The broken version, first

The natural failure mode: someone assumes a small, short-lived helper object is free
because "escape analysis will take care of it," never checks, and years later a
profiling session finds millions of these objects genuinely being allocated on the heap
because the object crossed a control-flow merge point or a non-inlined call boundary
the whole time — the assumption was wrong from day one, silently, because nobody ever
looked at an allocation profile to confirm it.

## 4. Interview follow-ups

- Why does this matter more at scale than it does in a quick local benchmark? A single
  request creating a few thousand "should have been free" objects is nothing. The same
  code path running millions of times a day in production turns a per-call cost nobody
  noticed into real, sustained GC pressure — the kind of gap that only shows up under
  real production load, not in a one-off local test.
- If an object graph is too deep or complex for escape analysis to fully prove
  non-escape, is there a code-level way to make it more likely to succeed? Simplifying
  the object graph, keeping hot-path methods small enough to inline reliably, and
  avoiding unnecessary boxing in hot loops all make it more likely the JIT's analysis
  can actually follow the object's full lifetime — none of these are guarantees, but
  they widen the cases the JIT is capable of proving.
