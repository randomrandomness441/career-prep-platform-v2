## ELI5: the photographer who can only shoot when everyone poses

Picture a camera that physically cannot take a photo unless every single person in the
kitchen first stops and holds a pose — no candid shots, ever. If someone spends most of
their shift moving fast between poses and never quite happens to be mid-pose when the
shutter is available, the camera will barely ever catch them, no matter how busy they
actually are. Meanwhile someone who dawdles between poses gets photographed constantly.
The camera isn't lying exactly, but its sampling is systematically skewed toward
whoever happens to pause a lot, not whoever's actually working the hardest.

## What you're actually building (understanding)

For a long time, most JVM profilers (including Java Flight Recorder, JFR) could only
capture a thread's stack trace at a **safepoint** — a specific checkpoint the JVM uses
for GC and other coordinated pauses. A thread deep inside a tight, safepoint-free loop
can run for a long time between safepoint checks, so a profiler that can only sample at
safepoints will systematically under-report exactly that code — not randomly, but
specifically biased against tight, hot loops, which is often the code you most want to
see.

`async-profiler` (which this pack actually used, for real, in question 009's Java
companion) exists specifically to avoid this. It uses `AsyncGetCallTrace`, an internal
HotSpot mechanism that can capture a thread's stack at any point, not just at a
safepoint, combined with Linux/macOS `perf_events` for native and kernel frames.

## Requirements

1. If a service has one hot method that's a tight loop with no safepoint checks, and
   another that calls out to I/O constantly (lots of natural safepoint opportunities),
   what would a safepoint-biased profiler get systematically wrong about their relative
   cost?
2. Why can't the JVM just add more safepoint checks everywhere to fix this? What would
   that cost, given what a safepoint actually requires (every thread reaching one before
   any thread can proceed)?
3. Given `async-profiler` avoids this specific bias, is it always the strictly better
   choice over JFR? Name one real reason a team might still choose JFR despite the bias.

## Why this matters

This is the profiling-methodology equivalent of the base pack's low-sampling-rate
lesson, but specific to how the JVM itself works — the tool you reach for can be
systematically wrong in a way that has nothing to do with how you read its output.
