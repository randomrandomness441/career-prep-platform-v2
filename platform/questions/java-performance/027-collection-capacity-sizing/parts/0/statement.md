## ELI5: buying a slightly-too-small suitcase

A slightly-too-small suitcase isn't a disaster — you just have to repack it once or
twice as you keep adding things, each time moving everything into a bigger one. It's
real, wasted effort, and it's also not the end of the world. Buying the exact right size
from the start avoids the repacking, but it was never going to ruin your trip either
way.

`ArrayList` and `HashMap`, given no capacity hint, grow the same way: start small,
and every time they run out of room, allocate a new, bigger backing array and copy
everything over. Telling them the real size up front avoids the repacking.

## What you're actually building (understanding)

Real, measured on this machine, JDK 21 — building a 100,000-element collection, 2,000
times, comparing default starting capacity against presizing exactly:

```
ArrayList default-capacity: 1185ms
ArrayList presized:         1086ms

HashMap default-capacity:   8718ms
HashMap presized:           8093ms
```

Presizing helps in both cases — about **9%** for `ArrayList`, about **7%** for
`HashMap`. Real, measurable, and worth doing in a hot path processing large
collections repeatedly. Also genuinely modest — nowhere near the order-of-magnitude
gaps found elsewhere in this pack (question 009's algorithmic fix, question 023's
exception cost, question 026's contention fix).

## Requirements

1. Given these numbers, is "always presize your collections, it's a huge performance
   win" an honest thing to tell a team? What would be a more accurate, calibrated
   version of that advice?
2. `HashMap`'s growth involves more than just copying an array — resizing means
   recomputing which bucket every existing entry belongs to, not just moving them.
   Given that, would you expect `HashMap` resizing to be proportionally cheaper, about
   the same, or more expensive than `ArrayList` resizing, for the same number of
   elements? Does the measured data support your reasoning?
3. If a codebase has hundreds of small, one-off collections (a handful of elements each,
   created once, used briefly, discarded) alongside a few genuinely large ones built
   repeatedly in a hot path, where should presizing effort actually go?

## Why this matters

Not every real, measurable optimization is dramatic, and reporting a 9% win as if it
were a 900% win erodes trust in every number that comes after it. Calibrating the size
of a real finding honestly is itself a real skill, separate from finding the
optimization in the first place.
