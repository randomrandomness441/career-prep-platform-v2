A good answer covers:

- **Why collapsing is a separate step.** Raw `perf script` output is one full stack
  trace per sample, printed as many lines of text, with no counting or merging done yet.
  `flamegraph.pl` expects one line per *unique* chain with a count already attached — it
  doesn't do any counting itself. Collapsing (deduplicating identical chains, summing
  their counts) has to happen first, or every sample would draw its own separate box
  instead of merging into one sized by frequency.
- **The sampling-rate estimate.** At 99Hz, one sample lands roughly every ~10ms. A
  function that runs for 2ms has roughly a 1-in-5 chance of being caught by any single
  sample near it — meaning at this rate you will frequently miss it entirely, and even
  when you do catch it, a handful of samples is a noisy signal, not a reliable measurement
  of a short-lived hot path. The answer doesn't need exact math, but should reason in
  the right direction: low sampling rate reliably misses short, fast functions, so
  "it's not in the flame graph" doesn't mean "it's not expensive."
- **Comparing across runs.** No, raw sample counts aren't directly comparable across
  two runs with different total samples — 1,200 out of 8,000 (15%) is not the same cost
  share as 1,200 out of 12,000 (10%). A good answer normalizes to a percentage of that
  run's total before comparing, or better, matches total wall-clock duration and sampling
  rate between the two captures in the first place.

NEEDS_WORK if the answer treats raw sample counts as directly comparable across two
different captures, or doesn't connect low sampling rate to missing short-lived functions.

## Code

**Inefficient/broken pipeline — skips collapsing, one hairline box per sample:**
```
perf record -F 99 -a -g -- ./my_program
perf script | flamegraph.pl > out.svg     # missing stackcollapse-perf.pl entirely
```

**Correct — collapse (dedupe + count) before drawing:**
```
perf record -F 99 -a -g -- ./my_program
perf script | stackcollapse-perf.pl | flamegraph.pl > out.svg
```

**Alternative — raise the sampling rate for a short, targeted capture, when hunting a specific short-lived function:**
```
perf record -F 999 -a -g -- sleep 10
perf script | stackcollapse-perf.pl | flamegraph.pl > out.svg
```

