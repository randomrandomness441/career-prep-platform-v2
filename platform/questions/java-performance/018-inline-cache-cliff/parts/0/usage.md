Plain text, three numbered points. Example shape:

```
1. The inline cache stores up to exactly two concrete receiver types with a
   direct comparison each -- that's why 1 and 2 types cost almost the same.
   A third type can't fit that two-slot cache, so the JIT abandons the fast
   comparison approach entirely for every future call at that site, which is
   why 3 types already costs as much as 4.
2. Most people guess "permanent." That guess is wrong -- measured here:
   mono (249.7ms) -> trimorphic (651.1ms) -> back to mono-only traffic
   (270.5ms), same process, no restart. It recovered. The mechanism is
   ongoing profile-guided recompilation (question 014) -- continued
   profiling can trigger a fresh compile once the pattern has genuinely
   changed again.
3. It doesn't matter on its own. The cliff depends on how many distinct
   types arrive at one specific call site, not how many implementations
   exist anywhere in the codebase. Two call sites each seeing only 1-2 of
   the 15 types get their own independent inline cache and both stay fast.
```
