## 1. Reframe

A CPU flame graph answers "where did the CPU spend its time." An off-CPU flame graph
answers a different question entirely: "where did threads spend time not running." They
sample different events (periodic CPU sampling vs. scheduler context-switch events) and
neither one substitutes for the other.

## 3. The broken version, first

The default instinct when something is slow is "profile it," which usually means "take
a CPU flame graph." That's the right first move when CPU usage is actually high. It's
the wrong move when CPU usage is normal or low and latency is still bad — that combination
is the signature of waiting, not computing, and a CPU flame graph is structurally blind
to waiting. Running it anyway, seeing nothing unusual, and concluding "profiling didn't
find anything" is a common dead end that off-CPU analysis exists specifically to avoid.

## 4. Interview follow-ups

- What real information does an off-CPU flame graph need that a CPU one doesn't? Scheduler
  events — specifically, when a thread gets switched off a core and when it gets switched
  back on — rather than a periodic timer sampling whoever's currently running.
- At extreme scale (tens of thousands of threads, high context-switch rate), what makes
  off-CPU tracing more expensive to run than CPU sampling? CPU sampling costs a fixed
  amount of overhead per timer tick regardless of load. Off-CPU tracing costs overhead
  proportional to the number of context switches, which scales with contention itself —
  the worse the problem, the more overhead the tool trying to diagnose it adds, which is
  a real production tradeoff to be aware of before turning it on broadly.
- This is a large part of why eBPF became the practical way to run off-CPU analysis in
  production rather than a research curiosity: it aggregates scheduler events *inside
  the kernel* before anything reaches userspace, instead of copying every single
  context-switch event out for a userspace tool to process one at a time — the
  aggregation cost scales with how much the profiler decides to keep, not with the raw
  event rate hitting the kernel.
