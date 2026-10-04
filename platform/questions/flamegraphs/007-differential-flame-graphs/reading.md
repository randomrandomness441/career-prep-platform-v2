## 1. Reframe

A differential flame graph answers "what changed," colored red for more expensive, blue
(or green, depending on the tool) for less. It doesn't answer "is the change good" —
that still requires knowing what the moved cost actually means in real terms.

Netflix's own 2015 writeup on building mixed-mode flame graphs lists automatic
differential comparison — collecting flame graphs over different days and diffing them
to catch a regression from a software change — as work they specifically wanted built,
not a solved problem even for a team that invented much of this tooling. That's worth
knowing: this isn't a mature, fully-automated capability everywhere, and treating it as
"the tool will just tell me if I regressed" overstates what it reliably does today.

## 3. The broken version, first

A common false victory: ship an optimization, take a differential flame graph, see a lot
of blue (less-expensive) where the old hot path was, declare success. Miss the red patch
that appeared somewhere else because the new approach shifted cost rather than removing
it. Differential graphs make the shift visible; they don't make the net effect obvious
by themselves — you still have to reason about whether the new cost is cheaper, safer,
or on a less-contended resource than the old one.

## 4. Interview follow-ups

- At extreme scale, is a sample-count difference of a few percent between two captures
  meaningful, or could it be noise? It could easily be noise — sampling has statistical
  variance, and two captures of literally identical code can differ by a few percent just
  from run-to-run variation. A good practice is running the comparison multiple times or
  over a longer capture window before trusting a small differential as a real signal.
- Why might two engineers, looking at the same differential flame graph, disagree about
  whether a change should ship? Because the graph shows sample-count movement, not
  business impact — one person might weigh "less CPU" as the win, another might weigh
  "more p99 latency variance from the new cache" as the loss, and the graph alone doesn't
  settle which matters more for this particular service.
