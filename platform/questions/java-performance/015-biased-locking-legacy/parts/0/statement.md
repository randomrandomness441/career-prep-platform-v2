## ELI5: everyone has to stop, just to change one name tag

A single employee has used the same desk, alone, for years — so the building stopped
bothering to check anyone in or out of that desk; it's just assumed to be theirs. One
day a second employee needs to briefly use that same desk. Before they can, building
security has to freeze the *entire building* — every employee, everywhere, not just the
two people near that one desk — just to safely update the desk's ownership record. Then
everyone resumes. The two employees involved barely noticed anything happened. Everyone
else in the building lost a moment of work for a change that had nothing to do with
them.

**Biased locking** (Java 6 through 14, deprecated in 15, off by default since 21) was an
optimization for the extremely common case of a lock object that's only ever acquired by
one thread — it let that thread skip the normal locking protocol entirely, assuming it
would keep being the only one. The moment a *second* thread tries to acquire that same
lock, the JVM has to **revoke the bias**, and revocation requires a **global safepoint**
— every thread in the JVM suspended, not just the two threads actually contending for
that lock.

## What you're actually building (understanding)

Documented real-world measurements: one case reported biased lock revocation accounting
for **14% of all stop-the-world time** in a running JVM. A separate real case (a
high-concurrency I/O benchmark using hundreds of threads) found the cost of a global
safepoint scales with the *total number of threads in the JVM*, not just the threads
involved in the specific lock — meaning the more concurrent the application generally
is, the worse an unrelated bias revocation gets.

## Requirements

1. Why does revoking bias on *one* object's lock require freezing *every* thread in the
   JVM, rather than just the two threads actually contending for that specific lock?
2. A service running on Java 11, with hundreds of threads, sees periodic, unexplained
   pauses that don't correlate with any GC log entries at all. What would make you
   suspect biased-locking revocation specifically, rather than GC (question 004) or
   cgroup throttling (question 007)?
3. This entire category of bug is not reproducible on a modern default JDK (biased
   locking has been off by default since Java 21). What real question should you ask
   before spending time chasing this specific cause in a production incident?

## Why this matters

A whole category of real, measured production pauses in this bug's era-of-relevance
never showed up in a GC log at all, because they were never a garbage collection event —
they were a locking-subsystem side effect that happened to require the same kind of
full-JVM freeze a GC pause does, for a completely unrelated reason.
