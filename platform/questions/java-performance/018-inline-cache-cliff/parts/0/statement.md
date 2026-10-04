## ELI5: the bouncer who knows two faces by heart

A bouncer at a members-only door has exactly one regular. The bouncer glances up, sees
that one familiar face, waves them in instantly — no list, no lookup, just recognition.
A second regular starts showing up too. Still fast: the bouncer now keeps two faces in
mind and does one quick "is it face A or face B" check — barely slower than recognizing
just one. Then a third regular arrives. The bouncer can't just add a third face to a
quick glance-check forever — at some point they give up on face recognition entirely
and pull out the actual member list, looking up *every single person* against it from
then on, including the original two regulars who used to get waved through instantly.

This is the real behavior of a Java interface call site, and it isn't a smooth,
proportional slowdown — it's a cliff at a specific number.

## What you're actually building (understanding)

Real, measured numbers from this machine, JDK 21: a tight loop calling `.area()` on
`Shape` objects through an interface reference, 200 million calls, after full JIT
warm-up on each fixed distribution (no type-switching after warm-up — that's a
different cost, covered in question 014):

```
monomorphic (1 type):   253.5ms
bimorphic   (2 types):  247.0ms
trimorphic  (3 types):  844.3ms
megamorphic (4 types):  765.0ms
```

Going from 1 type to 2 types: no meaningful change. Going from 2 types to 3 types: about
3.4x slower. Going from 3 to 4: no further meaningful change. This is HotSpot's real
inline-cache design: a call site caches up to **two** concrete receiver types with a
direct, fast comparison against each. The moment a **third** distinct type shows up, the
JIT gives up on that fast-path entirely and switches to a fundamentally different
dispatch strategy for every future call at that site — including calls to the original
two types, which lose their fast path too.

## Requirements

1. Why does the cost jump happen specifically between 2 and 3 types, rather than scaling
   up gradually as more types get added? What does that tell you about how many
   concrete types HotSpot's inline cache actually tracks before giving up?
2. Once a call site goes megamorphic, most engineers' first guess is "that's permanent
   for the life of the process — the fast path is gone for good." Before reading further,
   what would you have guessed? Then reason about what would actually have to happen,
   given question 014's recompilation model, for a call site to recover its fast path
   after traffic narrows back down.
3. A codebase has a `Shape`-like interface with 15 different implementations, but any
   single call site in the code only ever sees 1 or 2 of them in practice (a rendering
   pipeline handling only circles and squares in one code path, only triangles and
   rectangles in a completely different one). Does having 15 total implementations in
   the codebase matter for this specific performance cliff? Why or why not?

## Why this matters

"Polymorphism has a cost" is common wisdom stated so vaguely it's nearly useless. The
real, measured shape of that cost is a hard cliff at a specific number (two), not a
gradual tax that scales with how much polymorphism your design uses — which means the
actual engineering question is never "how much polymorphism," it's "how many concrete
types actually show up at this one call site in practice."
