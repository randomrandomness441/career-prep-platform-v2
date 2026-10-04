Plain text, three numbered points. Example shape:

```
1. Different. An if/instanceof chain is a literal sequence of checks in
   bytecode, evaluated in order every call. Pattern-matching switch resolves
   which case applies via one invokedynamic call to a typeSwitch bootstrap
   returning an index, then a real tableswitch jumps using that index --
   O(1) once the index is known, structurally not a sequential chain.
2. It rules out "fully sequential, checked one by one every call" -- a true
   linear scan across 10 types would show a much bigger gap than 14% between
   first and last position. It doesn't prove the mechanism is fully
   position-independent either -- the gap is real and repeatable, not noise,
   so I wouldn't claim to have fully verified the internal implementation
   from this alone.
3. Type-pattern dominance is a structural, decidable question from the type
   hierarchy alone -- the compiler can check it. A guard is an arbitrary
   boolean expression; proving two arbitrary expressions overlap is a
   general satisfiability problem the compiler doesn't attempt, so it just
   skips dominance checking for guarded cases entirely, even provably dead
   ones.
```
