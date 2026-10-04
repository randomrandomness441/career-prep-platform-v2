## ELI5: an actor who can only play one role until the play ends

Virtual threads (Java 21+) work like a theater with thousands of actors (virtual
threads) but only a handful of stages (real OS "carrier" threads, usually one per CPU
core). An actor waiting for their cue normally steps offstage so someone else can use
the stage while they wait — that's the whole trick, thousands of actors sharing a
handful of stages because most of them are waiting most of the time, not performing.

But if an actor is mid-scene holding a prop that the theater's rules say can't leave
the stage (a `synchronized` lock, on Java 21 through 23), and they have to pause and
wait for something, they can't step off. They're stuck standing on the stage, frozen,
holding the prop, and nobody else can use that stage until they finish. If enough
actors get stuck this way at once — one per stage — every stage is now occupied by a
frozen actor, and it doesn't matter how many other actors are ready in the wings. None
of them can go on. The theater has ground to a complete halt with a full house waiting.

## What you're actually building (understanding)

```java
synchronized (lock) {
    String result = slowHttpCall();   // blocks, waiting on the network
}
```

On JDK 21-23: a virtual thread executing this code blocks *while holding the monitor*
from `synchronized`. Because monitor ownership was tracked by the carrier thread's
identity (not the virtual thread's), the JVM can't safely unmount it mid-lock — the
virtual thread stays **pinned** to its carrier for the entire duration of `slowHttpCall()`,
and that carrier is unavailable to run any other virtual thread until it returns.

If your carrier pool is sized to 8 (the default, one per CPU core), and 8 concurrent
requests each hit this code path at the same moment, all 8 carriers are pinned. Every
one of the possibly thousands of other virtual threads waiting to run — including ones
that would finish in microseconds — gets zero carrier time until one of the 8 pinned
threads finishes its blocking call.

JDK 24's JEP 491 fixes most of this by changing how monitor ownership is tracked, so
`synchronized` blocks generally no longer pin.

## Requirements

1. Explain precisely why this failure mode is invisible if you only look at "how many
   virtual threads exist" or "how many requests are queued" — what's the actual
   constrained resource here, and how does it differ from a normal thread-pool-sizing
   problem?
2. If you suspected this was happening in production on JDK 21, name one concrete way
   to detect it (a real diagnostic mechanism, not "add print statements").
3. Two fixes exist: replace `synchronized` with `java.util.concurrent.locks.ReentrantLock`
   around the same critical section, or upgrade to JDK 24+. What's the real tradeoff
   between reaching for the code-level fix versus waiting for the JDK-level one, given
   a codebase with `synchronized` scattered across many blocking call sites?

## Why this matters

This is the newest bottleneck type in this whole pack — it didn't exist as a category
before virtual threads shipped in 2023, no classic profiling book covers it, and it
produces a failure mode (total throughput collapse with CPU usage looking fine) that
looks like several other things covered earlier in this pack before you know to check
for it specifically.
