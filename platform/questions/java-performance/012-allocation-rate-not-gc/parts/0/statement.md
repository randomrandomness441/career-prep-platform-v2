## ELI5: blaming the janitor for how much trash you make

A janitor empties the office trash cans every hour. One week, everyone starts throwing
out three times as much trash. The janitor now visibly spends more time emptying cans,
and takes slightly longer breaks between rounds to keep up. The obvious-looking
complaint is "the janitor got slower." The obvious-looking fix people reach for is
"hire a faster janitor" or "give the janitor a better cart." Neither addresses that the
actual change was upstream: people are generating three times as much trash, and no
janitor, however fast, makes that number smaller.

Garbage collection is the janitor. Spending "40% of wall time in GC" can mean the
collector is badly tuned — or it can mean the application is allocating so much garbage,
so fast, that no reasonable collector configuration would look good running it.

## What you're actually building (understanding)

Two services both show "GC overhead: 35% of CPU time" in monitoring. One team spends a
week tuning GC flags — heap size, region size, collector choice — and gets marginal
improvement. The other team profiles allocation instead of tuning GC, finds a single
hot path defensively copying a large list on every call when nothing downstream ever
mutates it, removes the unnecessary copy, and GC overhead drops from 35% to 6% with zero
GC-flag changes.

## Requirements

1. Before touching a single GC flag, what would you check first to tell these two
   situations apart — "badly tuned collector" versus "application allocating too much"?
   Name a concrete signal, not just "profile it."
2. Why does tuning GC parameters (heap size, region size, pause-time goals) have a hard
   ceiling on how much it can help when the real problem is allocation rate? What is GC
   tuning actually capable of changing, and what can it never change?
3. The team that removed the unnecessary defensive copy fixed the real problem in
   application code, not JVM configuration. Why is this fix specifically likely to be
   dismissed early in a debugging session, even by an experienced engineer who knows GC
   well?

## Why this matters

Time spent tuning a symptom instead of finding its cause is expensive twice: once as
wasted tuning effort, and again because the actual fix — usually smaller and simpler
than any GC flag change — stays undiscovered while everyone's attention is on the
collector.
