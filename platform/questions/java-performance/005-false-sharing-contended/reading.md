## 1. Reframe

Not every concurrency bug is a correctness bug. False sharing is real, measurable,
threads-interfering-with-each-other slowness that produces perfectly correct output and
sets off no race detector, because nothing about it is actually a race.

## 3. The broken version, first

This bug is uniquely hard to catch because every tool that's supposed to catch
concurrency problems is looking for the wrong thing. Functional tests pass — the output
is correct. A race detector finds nothing — there's no unsynchronized shared write. A
CPU flame graph shows normal-looking code — nothing is doing extra computational work.
The only symptom is "this is slower than it should be for what it's doing," which is
exactly the kind of vague complaint that's easy to dismiss or misattribute to something
else entirely.

## 4. Interview follow-ups

- Why is 64 bytes the number that matters here, and does it change across hardware?
  That's the typical cache line size on most modern x86 and ARM CPUs, but it's not
  universal — code relying on a hardcoded padding size for `@Contended`-style manual
  workarounds (before the annotation existed) could be wrong on hardware with a
  different line size, which is part of why `@Contended` (a JVM-level, portable
  mechanism) is preferable to manually inserting padding fields yourself.
- Would this bug get worse or better on a machine with more cores? Worse — more cores
  means more independent caches that can each hold a stale copy of the same cache line,
  and more cross-core invalidation traffic as each core's write forces the others to
  refetch. A bug that's barely measurable on 2 cores can become a real bottleneck on 64.
