Plain text, three numbered points. Example shape:

```
1. Integer.valueOf(100) returns the same cached object every call (100 is
   inside [-128,127]), so == (identity) is true. valueOf(1000) allocates a
   fresh object every call since 1000 is outside the cache, so two separate
   variables holding "1000" are two different objects -- == is false even
   though .equals() would say they're equal.
2. Tests using small numbers never leave the cache range, so every boxed
   value in every test run is a free cache hit -- the allocation cost is
   completely invisible in testing. Production data (real ids, counts,
   timestamps) is overwhelmingly outside the cache, paying real allocation
   cost every time, with zero code difference between the two.
3. The production/large-number path would show real allocation weight at the
   Integer.valueOf call sites. The small-number test path would show this as
   invisible or near-zero -- same shape as a correctly-escape-analyzed
   object from question 008, but the mechanism here is a value-based cache
   hit, not scalar replacement.
```
