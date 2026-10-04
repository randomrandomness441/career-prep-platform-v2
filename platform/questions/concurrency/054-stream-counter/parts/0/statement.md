# Stream Counter

## ELI5: a stadium with several turnstile counters, tallied at the end

A packed stadium doesn't count every single person through one turnstile, that would
create a massive line just to get in. It uses several turnstiles at once, each with its
own independent tally, and only adds all the tallies together when someone actually asks
"how many people are inside right now?", which happens rarely, compared to how often
people are walking through a turnstile.

This is the same idea from [[029-false-sharing]] and [[030-atomics-vs-mutexes]], put to
direct use: don't make every thread fight over one shared number on the hot path, give
each a cheap independent counter, and pay the cost of adding them up only when someone
actually wants the total.

## What you're actually building

Count every event in a high-volume stream, `increment()` is called extremely often,
from many threads; the total is read back only occasionally.

```cpp
class StreamCounter {
public:
    explicit StreamCounter(int num_shards);
    void increment();
    long total() const;
};
```

## Requirements

1. After any set of `increment()` calls has fully completed (all threads joined),
 `total()` must equal exactly the number of calls made, no lost updates, ever, under
 real concurrent load.
2. **`increment()` is the hot path**, it's called far more often than `total()`.
 Optimize for it, the way a stadium optimizes for the turnstile, not the head-count
 announcement.

## Why the constraints exist

**Use `num_shards`**, spread increments across multiple independent counters rather
than having every thread contend on one. That's the several-turnstiles idea, made
explicit as a constructor parameter you control.
