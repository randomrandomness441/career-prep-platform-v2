Plain text, three numbered points. Example shape:

```
1. It's an overgeneralization from older/other engines applied uncritically
   to modern java.util.regex -- the measured data directly contradicts
   "always" for these three classic patterns on this JDK.
2. "Partial mitigation" doesn't mean "solved everywhere." Treating a scoped
   fix as a universal guarantee is the same kind of unverified leap as the
   first claim, just pointed the other way.
3. The memoization keys on cursor position alone, assuming failing at a
   position means it'll always fail there. Backreferences break that --
   whether a later part of the pattern matches can depend on what an earlier
   group actually captured, not just where the cursor is, so "same position"
   isn't necessarily "same subproblem." Lookahead/lookbehind interacting with
   repetition is the same idea.
```
