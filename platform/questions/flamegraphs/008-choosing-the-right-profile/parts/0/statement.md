## ELI5: you weren't watching when it happened

Every question so far assumed you already had the right photos in hand. Real incidents
don't work that way — the kitchen catches fire at 2am, nobody was standing there with a
camera, and by the time you show up with one, the fire's already out and everything
looks calm. The hardest part of using a flame graph in practice usually isn't reading
one. It's realizing, in the moment, which kind of camera you needed, and whether you
even had one running before the problem happened.

## What you're actually building (understanding)

This question doesn't hand you data. It hands you three real scenarios. For each one,
decide: what kind of profile would you reach for (CPU flame graph, off-CPU flame graph,
differential flame graph, or an always-on continuous profiler like Pyroscope/Parca), and
why that one specifically rules out or beats the others for this exact situation.

**Scenario A.** A batch job that used to finish in 10 minutes now takes 25. It's fully
reproducible on demand in a staging environment, and CPU usage is pegged at 100% the
whole time it runs.

**Scenario B.** A production API's p99 latency spiked for eleven minutes last night at
3:14am and has been fine ever since. Nobody was running a profiler at the time. It has
not recurred since, and you don't know if or when it will again.

**Scenario C.** You shipped a change to your JSON serialization library two days ago.
Support tickets about "the app feels slower" have trickled in since, but nobody can
reliably reproduce it on demand, and you have last week's flame graph from before the
change sitting in a shared doc.

## Requirements

For each scenario, name the profiling approach you'd reach for first and justify it by
ruling out at least one alternative — not just "why this works" but "why the other
approaches don't fit this specific situation as well."

## Why this matters

Every technique in this pack is easy to explain in isolation. The actual skill senior
engineers get paid for is picking the right one under time pressure, for an incident
that doesn't announce in advance which kind it's going to be.
