## ELI5: eight people fighting over one shared notebook

Eight people each need to write down the next number in a sequence, constantly, all
day. Give them one shared notebook: every single person has to grab it, check what the
last number was, write the next one, and let go — and if someone else grabbed it in the
meantime, they have to throw away their attempt and try the whole thing again. With
eight people all reaching for the same notebook nonstop, most attempts fail and have to
be redone, and the failed attempts themselves get in the way of everyone else's next
attempt too. Give each person their own notebook instead, and nobody ever waits on
anybody.

`java.util.Random` is the shared notebook — its internal seed update uses a
compare-and-swap loop, and *every* thread sharing one `Random` instance fights over that
same seed. `ThreadLocalRandom` is everyone getting their own notebook.

## What you're actually building (understanding)

Real, measured on this machine, JDK 21, 8 threads, 20 million `nextInt()` calls per
thread:

```
shared Random,       8 threads: 51090ms
ThreadLocalRandom,   8 threads:    71ms
```

**About 719x.** Not a typo, and not a made-up worst case — a genuinely measured
CAS-retry collapse from 8 threads hammering one shared mutable seed.

## Requirements

1. A single-threaded program using a shared `Random` instance has no contention problem
   at all — one thread, no competition for the seed. Why does this specific cost only
   show up under real concurrent access, and why might it be invisible in testing if
   your tests don't exercise realistic thread counts?
2. Compare-and-swap (CAS) contention under heavy load can degrade *worse* than a plain
   lock would under the same contention. Reason about why: what does a thread do when a
   lock is already held, versus what a thread does when its CAS attempt fails?
3. `ThreadLocalRandom.current()` gives each thread its own independent generator state.
   Does this mean two different threads calling `ThreadLocalRandom.current().nextInt()`
   at the same moment could ever produce the exact same sequence of numbers as each
   other? Why does the answer matter for whether this class is a safe drop-in
   replacement for every use of shared `Random`.

## Why this matters

This is a textbook case of a bottleneck invisible in code review: `Random` is a
completely ordinary, correct, thread-safe class. Nothing about `new Random()` shared as
a field looks alarming. The cost only exists in the specific combination of real thread
count and real call frequency — exactly the kind of thing that only shows up under
production load, never in a quick local test.
