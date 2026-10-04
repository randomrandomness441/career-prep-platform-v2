## 1. Reframe

Every flame graph is downstream of two lossy steps: how often you sampled, and how well
the profiler could name what it saw. Both can make a real cost invisible or unlabeled
without the graph ever looking obviously broken.

## 3. The broken version, first

The natural failure mode is treating a clean-looking flame graph as ground truth. A graph
with no obvious `[unknown]` boxes and smooth-looking widths feels trustworthy, but a low
sampling rate produces exactly that kind of clean-looking graph while quietly missing
every short, rare event that never happened to land in a sample window. There's no visual
cue in the graph itself that tells you what it missed — you have to know the capture
parameters to know what's structurally invisible to it.

A third, separate failure mode from missing symbols or low sampling rate: a **broken
stack itself**. Compilers routinely reuse the frame-pointer register for something else
as a speed optimization (`gcc`/`clang` do this by default), which breaks the classic way
a profiler walks the stack — the profiler can get a *partial* or *wrong* call chain, not
just an unlabeled one. This was a real, unsolved problem for profiling Java in production
until 2015: Java had no equivalent of C's `-fno-omit-frame-pointer` flag, so a system
profiler like Linux's `perf` genuinely could not walk a Java stack correctly at all.
Brendan Gregg patched OpenJDK himself to add one, submitted it upstream, and it shipped
as the real JDK option `-XX:+PreserveFramePointer` (JDK8 update 60 and later) — which is
also what makes "mixed-mode" flame graphs possible: kernel, native, and JVM frames all
correctly walked and shown in one picture, something no single profiler could do before
that fix existed. Preserving the frame pointer costs a small amount of performance itself
(measured at roughly 0-3% CPU, depending on the workload) — another real instance of this
pack's running theme: turning on better visibility is not free, and the cost is worth
measuring, not assuming.

## 4. Interview follow-ups

- Why would you deliberately choose a *lower* sampling rate in some production settings,
  even knowing it misses short events? Sampling overhead scales with rate — very high
  frequency profiling can itself perturb the very performance you're trying to measure,
  especially at scale across many hosts. There's a real tradeoff between resolution and
  observer effect.
- JVM profilers specifically call out "safepoint sampling bias" — profiling only being
  able to sample at JIT safepoints, which biases results toward code that happens to have
  safepoints nearby. Why does a language runtime detail like this matter to someone who
  just wants a flame graph? Because the bias isn't random noise, it's systematic: certain
  code patterns are structurally more or less likely to be sampled regardless of their
  actual cost, so two functions with identical real cost can show up with different
  widths purely because of where safepoints happen to fall in their compiled code.
