## ELI5: hiring based on the building's capacity, not your office's

Imagine a company hires cleaning staff based on the size of the entire office building
they're in — a hundred floors, so they hire a hundred cleaners. But this company only
actually rents and pays for two floors. All hundred cleaners show up, try to work at
once, and immediately get turned away at building security because the lease only
covers two floors' worth of access at a time. The hiring number was based on the wrong
thing: the building's total size, not what was actually allocated to this tenant.

A JVM historically sized its internal thread pools (garbage collection threads, the
`ForkJoinPool` used by parallel streams) by asking the operating system "how many CPU
cores exist on this machine" — the whole host's core count, not the container's actual
CPU quota. On a 64-core host with a container limited to 2 CPUs, a GC pause could try to
run dozens of GC threads simultaneously, all competing for a 2-CPU allowance, burning
through the container's entire CPU quota for that scheduling period in milliseconds and
getting throttled by the kernel's CFS scheduler for the rest of it.

## What you're actually building (understanding)

`-XX:+UseContainerSupport` (default since JDK 8u191) tells the JVM to read the
container's actual cgroup limits instead of the host's. Two real remaining traps even
with it enabled, on a modern JDK:

- **Fractional CPU limits.** A container limited to 2.5 vCPUs still gets rounded to a
  whole-number thread-pool size internally, which can under- or over-provision relative
  to the real quota.
- **No CPU limit set at all.** Removing the limit removes the signal the JVM uses to
  size its pools sensibly — it falls back to seeing the whole host's core count again,
  the exact problem `UseContainerSupport` exists to prevent.

## Requirements

1. Explain concretely why a GC pause specifically (not steady-state request handling)
   is where this problem shows up hardest, given what a stop-the-world pause actually
   does with its thread pool.
2. A service is deployed with a 2-CPU limit but no `-XX:ActiveProcessorCount` override,
   on a JDK with `UseContainerSupport` already on by default. It's still seeing periodic
   latency spikes that correlate exactly with GC events, worse than an identical service
   with a 4-CPU limit. What would you check first, given `UseContainerSupport` alone
   isn't a complete guarantee?
3. If this is caused by CPU throttling, would a CPU flame graph taken during a spike
   show high CPU usage, low CPU usage, or something else entirely? Justify it from what
   throttling actually does to a process.

## Why this matters

This bug doesn't live in application code at all — it lives in the gap between what the
JVM believes about its environment and what the container orchestrator actually grants
it. No amount of staring at your own business logic finds this.
