## ELI5: the borrowed tool that never has to leave the room

If you borrow a tool, use it in one room, and hand it straight back before you ever
leave that room, nobody watching from outside needs to track where it went — it never
"escaped" the room. If instead you carry it into the hallway and pass it to someone
else, now it genuinely has to exist as a real, trackable object, because it left.

The JIT's **escape analysis** does exactly this for objects: if it can prove an object
never "escapes" the method that created it (never gets stored somewhere else, never
gets returned, never gets handed to something that might keep it), it can skip
allocating it on the heap entirely — keep its fields as plain local variables instead
("scalar replacement"), no garbage, no GC pressure, as if the object never existed as a
real object at all.

## What you're actually building (understanding)

This is genuinely powerful and genuinely limited. Real, documented limits on what
escape analysis can see through:

- An object passed to a method call that doesn't get inlined — the analysis can't see
  inside a call it didn't inline, so it has to assume the object might escape.
- An object created on one branch and merged with another value at a control-flow join
  point — the analysis often can't prove non-escape across the merge.
- Autoboxing in generic code (`List<Integer>` boxing a primitive `int` into a real
  `Integer` object) — frequently defeats scalar replacement even when the boxed value
  never leaves a tight scope.
- Any object graph or inlining chain that exceeds the JIT's internal budget for how
  deep it's willing to analyze.

Because of this, "just trust the JIT to handle small object allocations" is often true
and sometimes badly wrong, and the only way to know which is profiling — specifically,
an **allocation flame graph** (async-profiler's allocation mode, or JFR's allocation
events), not a CPU flame graph.

## Requirements

1. If escape analysis is working correctly for a small helper object, what would an
   allocation flame graph show for it — present with real weight, or effectively
   invisible? Explain why.
2. Allocation profiles built from TLAB (thread-local allocation buffer) sampling report
   size, not count, by default. If a hot path allocates millions of tiny boxed
   `Integer` objects and, separately, something else allocates one enormous byte array
   occasionally, which would look "bigger" in a size-sorted allocation flame graph, and
   why might that ranking mislead you about which one actually matters more for GC
   pressure?
3. Name one concrete way to test whether a specific object is actually being
   scalar-replaced or not, beyond just assuming the JIT is handling it.

## Why this matters

"The compiler optimizes that away" is one of the most common unverified assumptions in
Java performance work — sometimes true, sometimes false in a way that costs real GC
pressure at scale, and the only way to know which is measuring it.
