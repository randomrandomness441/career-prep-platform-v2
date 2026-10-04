## 1. Reframe

The y-axis is a call stack, snapshotted. Bottom = who started it all. Top = who was
actually running when the snapshot happened. Everything in between was waiting on the
frame above it to return before it could continue.

## 3. The broken version, first

A common early mistake: looking at a tall stack of frames and assuming the cost is
spread across all of them, or worse, blaming the frame in the middle because it has a
familiar-sounding name. Only the top frame of each individual stack sample was ever
actually executing. A frame lower down is only "guilty" in the sense that it chose to
call something expensive — the fix for a hot `malloc` at the top might be in `malloc`
itself, or it might be in `json_alloc` for calling `malloc` too often. Both are valid
fixes; conflating "who's on top" with "who's to blame" is not.

## 4. Interview follow-ups

- If `main;a;b;c` has 5 samples and `main;a;b;d` has 5 samples, how wide is `b`'s box?
  10 (5+5) — a parent's width is the sum of its children's, always, because every
  sample under a child is also a sample under everything that called it.
- A stack trace shows the same function appearing twice at different depths
  (`main;recurse;recurse;recurse`) — what does that tell you, and does it change how
  you read the top frame? It's recursion. The rule doesn't change: whichever occurrence
  is topmost is still the one actually executing at that sample.
- Two functions are equally wide in a plain CPU flame graph. Does that mean they cost
  the same? Not necessarily — width only counts *presence* in samples, not how
  efficiently that time was spent. A **CPI flame graph** (cycles-per-instruction, a
  variant Brendan Gregg has demonstrated) colors or sizes frames by how many CPU cycles
  they burned per instruction retired, which can reveal that one "equally wide" frame
  was doing real work while the other was stalled on cache misses the whole time —
  on-CPU and efficient are two different claims, and plain width only ever tells you
  the first one.
