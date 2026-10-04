## ELI5: two cooks, two cutting boards, one shared tray

Two cooks each have their own cutting board and never touch each other's ingredients.
Correct, no conflict, no need to coordinate. But both boards sit on the same serving
tray, and every time either cook so much as taps their board, the tray wobbles and the
other cook has to steady their own board before continuing. They're not sharing any
ingredients. They're sharing the tray, and that's enough to slow both of them down.

CPUs load and invalidate memory in fixed-size chunks called cache lines (64 bytes on
most hardware), not one variable at a time. If two threads' *independent, unrelated*
counters happen to land in the same 64-byte chunk, every write to one invalidates the
whole line for the other core's cached copy — even though the two threads never touch
each other's actual data. That's false sharing.

## What you're actually building (understanding)

```java
class Counters {
    volatile long readCount;    // thread A increments this
    volatile long writeCount;   // thread B increments this, completely unrelated to A
}
```

Two fields, two threads, zero logical dependency between them. If they happen to land
in the same cache line (likely here — two adjacent `long` fields in one small object),
every increment from thread A can force thread B's core to re-fetch the line, and vice
versa, purely from proximity in memory.

Java 8 added `@Contended` (JEP 142) specifically for this: annotating a field tells the
JVM to add padding around it so it lands in its own cache line, at the cost of using
more memory for the padding.

## Requirements

1. Why does adding `synchronized` or any other lock around these two fields *not* fix
   false sharing, even though it would fix a real data race? (There's no race here to
   begin with — walk through why that's true.)
2. If you profiled this with a CPU flame graph, what would it actually show you, and
   why is that specifically unhelpful for diagnosing false sharing? What signal would
   actually reveal it (think about what's different about this cost versus normal
   computation).
3. `@Contended` fixes this by adding padding. Under high contention, `LongAdder` outperforms
   a single contended `AtomicLong` counter by striping the count across multiple internal
   cells. Is `LongAdder`'s fix the same idea as `@Contended`'s, a different one, or does
   it use the same underlying technique for a different reason?

## Why this matters

This bug produces zero incorrect output, zero TSan-style race warnings, and looks
completely correct under any functional test. It only ever shows up as "this is
mysteriously slower than the math says it should be" — a category of bug that pure
correctness testing structurally cannot catch.
