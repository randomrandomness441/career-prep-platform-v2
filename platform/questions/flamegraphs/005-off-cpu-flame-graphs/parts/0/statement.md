## ELI5: the camera only sees people working

The hidden camera has one rule: it only photographs someone if they're actively doing
something. A cook standing at the pass, arms crossed, waiting for the walk-in fridge
door to be free because someone else is in there — the camera doesn't count that as
"work," so it doesn't show up in any photo. If half your kitchen staff spend half the
shift standing around waiting for the fridge, your photos will show a kitchen that
looks totally fine: everyone captured is genuinely busy, all the time. The photos just
never catch the people standing still.

That's exactly what a normal (CPU) flame graph does. It only samples threads that are
actually running on a CPU core right this instant. A thread that's blocked — waiting
on a lock, waiting on a disk read, waiting on a network reply, sleeping — burns zero
CPU while it waits, so it never gets sampled, so it's invisible to a CPU flame graph
no matter how long it waited.

## What you're actually building (understanding)

A service's P99 latency doubles after a deploy. You take a CPU flame graph. It looks
almost identical to last week's — same functions, same widths, nothing obviously wider.
CPU usage on the box hasn't gone up either. But users are timing out.

An **off-CPU flame graph** samples the opposite thing: instead of "who's on a CPU core
right now," it captures "who just got taken *off* a CPU core, and how long were they
gone before they came back" — built from scheduler events, not periodic CPU sampling.
Where a CPU flame graph's frames are sized by time-spent-running, an off-CPU flame
graph's frames are sized by time-spent-waiting.

## Requirements

1. Explain why a CPU flame graph can look completely unchanged while P99 latency
   doubles. What kind of new cost would be invisible to it?
2. If the real cause turns out to be a mutex that's now heavily contended (many threads
   fighting over the same lock), which flame graph — CPU or off-CPU — would actually
   show that, and what would the frame at the top of the wide box be? (Think about what
   a blocked thread is doing while it waits: it's inside some lock/wait function, even
   though it's not running.)
3. Would taking a *longer* CPU-graph capture (say, 10 minutes instead of 1) fix the blind
   spot in part 1? Why or why not?

## Why this matters

Reaching for a CPU flame graph by default and stopping there is the single most common
reason a real production latency regression goes undiagnosed — the tool you reached for
was never designed to see the kind of cost that's actually happening.
