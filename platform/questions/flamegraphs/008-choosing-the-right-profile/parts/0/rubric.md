A good answer covers, per scenario:

- **Scenario A — CPU flame graph.** Reproducible on demand, and CPU pegged at 100% means
  the cost is genuinely computational, not waiting. A CPU flame graph is the direct tool:
  take one while it runs, find the widest path. Off-CPU would be the wrong reach here —
  there's no evidence of blocking, and off-CPU analysis answers a question (where is time
  spent waiting) that isn't the one this scenario is asking.
- **Scenario B — this is the case a normal CPU/off-CPU flame graph *cannot* answer after
  the fact**, because nobody was capturing at 3:14am and the problem hasn't recurred on
  demand. The right answer is a continuous/always-on profiler (Pyroscope, Parca, or
  equivalent eBPF-based fleet-wide profiling) that was (or should have been) running
  before the incident, so the 3:14am window can be queried retroactively. A good answer
  explicitly says "you can't profile the past with a tool that wasn't running" and names
  continuous profiling as the thing that would have prevented this dead end next time —
  NEEDS_WORK if the answer proposes taking a fresh CPU or off-CPU capture now, since the
  problem already stopped recurring.
- **Scenario C — differential flame graph**, comparing last week's saved profile against
  a fresh one taken now, specifically because there's a known before/after boundary (the
  serialization change) and a saved "before" already exists. This directly isolates
  whether the new library changed the cost profile, rather than a plain CPU graph which
  would only show today's state with no baseline to compare against. A good answer notes
  that if reproduction is unreliable, they'd want several fresh captures (or ideally a
  continuous profiler covering the rollout window) to be confident the difference isn't
  just run-to-run noise, tying back to question 007's caveat about noise vs. signal.

NEEDS_WORK if any scenario's chosen tool doesn't match the actual constraint that rules
out the alternatives (reproducibility, whether CPU is pegged, whether a "before" exists).

## Configuration — the right command per scenario

**Scenario A, wrong reach — off-CPU when the evidence points at pure computation:**
```
perf record -e sched:sched_switch -a -g -- sleep 30    # nothing to see, CPU is pegged, not blocked
```
**Scenario A, correct:**
```
perf record -F 999 -a -g -- sleep 30
perf script | stackcollapse-perf.pl | flamegraph.pl > cpu.svg
```

**Scenario B, wrong reach — a fresh capture, after the incident already ended:**
```
perf record -F 999 -a -g -- sleep 30    # too late; nothing was running at 3:14am
```
**Scenario B, correct — should have been running continuously already:**
```
# fleet-wide, always-on -- query the 3:14am window retroactively
parca-agent --node-name=$(hostname)
```

**Scenario C, correct — diff against a saved baseline:**
```
diff_folded.pl before.collapsed after.collapsed | flamegraph.pl > diff.svg
```

