## 1. Reframe

A flame graph is the last step of a three-step pipeline: sample the stack repeatedly,
collapse identical chains into counted lines, draw boxes sized by those counts. The
folded-stack format (`func;func;func count`) is the actual interchange point almost every
tool uses, which is why the same drawing tool works for Java, Python, Go, and C++ profiles
alike — they all get converted into the same simple text format first.

## 3. The broken version, first

Someone builds a homemade profiler that prints the current stack every time it fires, one
full trace per line, and feeds that straight into a graphing library expecting pre-counted
folded data. Every sample draws as its own separate, unmerged box. The resulting "flame
graph" is a wall of thousands of hairline-thin bars that never combine into a readable
shape — because nothing ever summed the duplicates. The fix is always the same: collapse
first, count, then draw.

## 4. Interview follow-ups

- Real production systems now often skip the "record to a file, then post-process" model
  entirely and profile continuously — tools like Pyroscope and Parca run an always-on
  eBPF-based collector across a whole fleet and store pre-aggregated profiles centrally,
  so you can query "what did this service's CPU profile look like at 2am last Tuesday"
  without having had a `perf record` running at that exact moment. The folded-stack model
  is still the underlying data shape; continuous profiling just automates the collection.
- Why does eBPF-based profiling (Parca Agent, Grafana Alloy) not need code changes to the
  profiled program, while something like async-profiler is attached per-process? eBPF
  profilers hook into the kernel scheduler/perf subsystem and can walk any process's stack
  from outside it; per-process profilers need to run inside or be attached to that specific
  process's runtime to read its stack correctly (especially for managed runtimes like the
  JVM, where a naive stack walk sees native frames, not the Java method names).
- Why is eBPF specifically what made off-CPU and differential flame graphs practical at
  fleet scale, not just possible in a lab? Both need aggregation across huge numbers of
  events (every context switch, or every sample across thousands of hosts) — doing that
  aggregation in-kernel, before handing a much smaller summarized result to userspace, is
  what keeps the overhead low enough to run continuously in production instead of only
  during a short, deliberate capture window.
