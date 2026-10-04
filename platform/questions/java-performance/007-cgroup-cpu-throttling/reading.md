## 1. Reframe

Some of the worst Java performance bugs live entirely outside the JVM's own decisions —
in a mismatch between what the JVM believes about the machine it's running on and what
a container orchestrator has actually granted it.

## 3. The broken version, first

Before `-XX:+UseContainerSupport` existed (and still, in the fractional/unset-limit
edge cases it doesn't fully cover), a JVM would ask the OS for the host's total core
count and size GC and `ForkJoinPool` threads accordingly — completely reasonable
behavior for a JVM running directly on bare metal, and completely wrong for one running
in a 2-CPU-limited container on a 64-core host. The bug isn't in any line of Java code;
it's in an assumption the JVM makes about its environment that stopped being true the
moment containers entered the picture.

## 4. Interview follow-ups

- Why does this problem get *worse*, not better, on higher-core-count host machines,
  even though the container's own CPU limit hasn't changed? Because the JVM (without
  correct container awareness) sizes its pools off the host's core count — a 2-CPU
  container on a 64-core host sees a JVM trying to spin up far more threads than one
  on an 8-core host would, making the burst-then-throttle collision more severe as host
  size grows, even though the actual allocated quota is identical.
- Kubernetes CPU *requests* vs. *limits* — why does a pod with a CPU request but no
  limit still leave the JVM's sizing logic in a worse position than one with an explicit
  limit? Without a hard limit, there's no cgroup quota signal for `UseContainerSupport`
  to read at all — the JVM falls back to seeing the full host, the exact failure mode
  this flag exists to prevent, even though the pod is still implicitly constrained by
  cluster scheduling in practice.
