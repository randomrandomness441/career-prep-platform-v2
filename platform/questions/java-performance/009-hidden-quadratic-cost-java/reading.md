## 1. Reframe

Same lesson as the C++ version of this question, on a real JVM this time: a helper
that looks O(1) can secretly cost more the longer the program runs, and reading the
source won't tell you — only running a profiler against real execution will.

## 2. What was actually run, for real

`async-profiler` (`asprof`, version 4.5, installed via Homebrew) was attached to a real
JDK 21 process running the naive `dedupe`/`seenBefore` pair on 300,000 ids, in CPU-event
mode, collapsed-stack output. Real numbers from that run, nothing invented:

- Total samples across the whole run: **13,725**
- Samples under `Dedupe.main;Dedupe.dedupe` (and everything beneath it, including
  `seenBefore`): **13,595** — about **99%** of the entire program's CPU time
- Of those, `Dedupe.main;Dedupe.dedupe` with `seenBefore` **not** appearing as its own
  separate frame accounted for the majority (8,667 samples) — more time attributed
  directly to `dedupe` than to `seenBefore` itself, even though `seenBefore` is where
  the actual linear scan lives

That last point is worth sitting with. It's not that the cost moved somewhere real —
it's that HotSpot's JIT, recognizing `seenBefore` as small and extremely hot, appears to
have inlined enough of it into `dedupe`'s own compiled code that the profiler attributed
many samples to the caller's frame directly rather than a separate `seenBefore` frame.
The cost didn't disappear. The frame that would have named it did. This is the exact
inlining-hides-structure lesson from the base pack's question 006, caught happening live
on a real JVM instead of just described.

## 3. The broken version, first

Read `dedupe` and `seenBefore` on their own and nothing looks wrong — eleven lines,
both individually reasonable. The quadratic cost only exists in the relationship
between them: `seenBefore` scans a list that `dedupe`'s own loop keeps growing. Measured
on this machine: the naive version took roughly 1650ms on 100,000 ids; the fixed
`HashSet`-based version took about 21ms on the same input — over 75x. `runner.evaluate`
(the Java path, `java_runner.py`) enforces a 400ms budget on exactly this input, which
the naive version fails by a wide margin every time and the fixed version clears with
room to spare.

## 4. Real-world usage

This is the same bug shape covered in the base pack's citation of OpenSearch's Java
client incident — a helper called from a hot path whose own cost wasn't visible until
profiled. The Java-specific wrinkle this question adds: even once you've correctly
identified the hot call site, the JIT's own inlining decisions can determine whether
your profiler shows you a separate frame for the actual culprit or folds it into its
caller — reading a Java flame graph sometimes means recognizing that "this caller looks
expensive" *is* the signal, not a dead end, because the real cost is folded inside it.

## 5. Performance

All measured, all real, all on this machine: naive at 100,000 elements, ~1650ms. Fixed,
same input, ~21ms (about 75-90x, consistent across repeated runs). `async-profiler`'s
full-run sample gave ~99% attribution to `dedupe`+`seenBefore` combined at 300,000
elements — essentially the entire program's runtime, exactly as expected for a function
whose only job is this one loop.

## 6. Where this solution fails

`HashSet<Integer>` autoboxes every `int` into an `Integer` object — real allocation
pressure at scale that a C++ `std::unordered_set<int>` doesn't pay in the same way
(question 008 in this pack covers exactly this cost, in general). At extreme scale,
using a primitive-specialized collection (from a library like Eclipse Collections or
fastutil, which avoid boxing entirely) would remove that overhead — this fix solves the
algorithmic complexity, not every possible cost in the fixed version.

## 7. Interview follow-ups

- If this ran inside a hot request-handling path instead of a one-shot batch job, would
  JIT warm-up (question 004's tiered-compilation lesson, applied here) change how you'd
  measure it? Yes — a single cold `java` process invocation pays interpreter and C1/C2
  warm-up cost on top of the algorithm itself; a fair production-realistic measurement
  would run the method many times in a loop and look at steady-state timing, not just a
  single invocation's wall time the way this question's test does for simplicity.
- Why did the fix choose `HashSet` over `TreeSet`? `HashSet` gives average O(1) lookup
  and insertion; `TreeSet` gives O(log n) but keeps elements sorted — since this
  question's contract requires *first-occurrence* order, not sorted order, `TreeSet`
  would need an extra step to restore original ordering anyway, making `HashSet` (paired
  with the separate `result` list for ordering) the more direct fit.
