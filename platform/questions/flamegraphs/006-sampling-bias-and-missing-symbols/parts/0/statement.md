## ELI5: a camera with a slow shutter and no name tags

Two problems with the kitchen camera, separate from anything covered so far.

First: it only clicks once every ten seconds. If the fire alarm panic — everyone
sprinting for thirty seconds — happens between two clicks, the camera simply never
catches it. Not "catches it faintly." Never catches it at all. A rare shift-defining
event can leave zero photos.

Second: some of the staff wear ski masks (new hires the payroll system hasn't
processed their name tags for yet). The camera photographs them fine, but the picture
just says "person" instead of a name. And sometimes the kitchen manager, to save time,
merges two people's jobs into one name tag on the schedule — "prep" now secretly
means "chop AND plate," and you'd never know two separate jobs are hiding under one
label unless you asked.

## What you're actually building (understanding)

Real, common versions of both problems:

- **Sampling frequency**: `perf record -F 99` samples 99 times a second (~10ms apart).
  A lock held for 3ms, itself called rarely, has a real chance of never landing inside
  any sample window at all across a whole capture.
- **Missing symbols**: a flame graph with a big box labeled `[unknown]` usually means
  the binary or a shared library it called into was stripped of debug symbols — the
  profiler captured the address on the stack but can't map it to a function name.
- **Inlining**: aggressive compiler optimization (or the JIT, for a JVM) can merge
  several small functions into one native frame. Two logically separate functions in
  your source code can show up as a single frame in the profile, with no visible
  boundary between them.

## Requirements

1. You suspect a specific, short (sub-5ms), infrequent lock acquisition is costing you
   latency, but it never appears in a 99Hz CPU flame graph at all — not small, just
   absent. What's the most direct fix to actually catch it, and why does it work?
2. You see a 15%-wide `[unknown]` box in a production flame graph. Name one real reason
   this happens and one concrete thing you'd do about it before concluding "there's
   nothing to profile there."
3. Someone says "our flame graph shows `handle_request` is one big 40%-wide box with no
   further breakdown underneath it — that whole function must genuinely take that long
   to run, line by line." What alternative explanation should you consider before
   accepting that at face value?

## Why this matters

A flame graph that looks clean and confident can still be systematically wrong in ways
that have nothing to do with reading it correctly — the data underneath it can simply be
missing or merged. Knowing when to distrust the picture is as important as knowing how
to read it.
