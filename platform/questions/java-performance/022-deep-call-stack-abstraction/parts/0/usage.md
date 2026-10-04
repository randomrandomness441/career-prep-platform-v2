Plain text, three numbered points. Example shape:

```
1. Not a contradiction -- two regimes of the same curve. Below the inlining
   budget, a wrapper chain gets flattened into one compiled unit, matching
   "nearly free." Past the budget, inlining stops and every call beyond it
   pays real dispatch cost, matching the measured ~3x jump.
2. HotSpot compiles hot methods with C1 first (with profiling), only
   promoting to C2 after enough invocations. If the C1-compiled version --
   built with C1's shallower 9-level budget -- is what's doing the work
   during the measured window, the result reflects C1's limit rather than
   C2's higher one. A separate total-code-size inlining budget being
   exhausted around the same depth is an equally plausible alternative.
3. Two separate risks. No TCO means a recursive walk pays one real stack
   frame per nesting level with a real, finite ceiling (~32,800 here),
   completely independent of whether the surrounding static layering itself
   got inlined away. A codebase could have cheap, fully-inlined layering and
   still hit StackOverflowError from a deeply recursive data structure --
   different problems that happen to share "the call stack."
```
