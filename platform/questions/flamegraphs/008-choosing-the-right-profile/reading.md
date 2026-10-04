## 1. Reframe

Every earlier question in this pack assumed you already had the right capture in hand.
The actual hard part in production is upstream of all of that: deciding what to capture,
when, and realizing that some incidents can only be answered by something that was
already running before you knew you'd need it.

## 3. The broken version, first

The default instinct under pressure is "just take a flame graph" — meaning, almost always,
a fresh CPU capture, taken now. That's exactly the tool that fails scenario B: an incident
that already ended, with nothing running at the time to have captured it. Reaching for the
familiar tool regardless of fit is the actual failure mode this capstone is testing, not
any single technical misunderstanding from the earlier questions.

## 4. Interview follow-ups

- Why has continuous profiling (Pyroscope, Parca, Grafana Alloy's eBPF mode) become
  standard in mature production setups rather than a nice-to-have? Because scenario B is
  common — most real performance incidents are not reproducible on demand, and the only
  way to ever answer "what was happening at 3:14am" is to already have been sampling
  before you knew you'd need to ask.
- What's the cost of running continuous profiling fleet-wide all the time, and why doesn't
  every team just turn it on by default? Overhead (CPU and storage for continuously
  collected samples across every host) and the operational cost of running and maintaining
  another piece of infrastructure — the eBPF-based tools specifically reduce per-process
  overhead compared to older continuous-profiling approaches, which is part of why they've
  become the default choice, but the tradeoff against "just profile when something breaks"
  is real and worth weighing against how often incidents like scenario B actually happen
  for a given service.
- This pack only covered four kinds of profile (CPU, off-CPU, differential, continuous).
  Real toolkits go further — CPI (cycles-per-instruction) flame graphs surface code that's
  hot *and* inefficient per instruction, not just hot; FlameScope-style subsecond heatmaps
  show *when* within a capture window a problem happened, which a single merged flame graph
  averages away entirely. The actual skill this capstone is testing scales past four options:
  knowing a wider toolkit exists, and reaching for the one that matches the actual question
  being asked, not the one you happen to remember first.
