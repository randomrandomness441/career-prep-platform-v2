# Lock-Free SPSC Ring Buffer

## ELI5: the mailbox flag that goes up too early

A rural mailbox has a little flag: you put a letter in, then raise the flag to say "mail's
here." Your neighbor, walking by, only ever checks the flag, if it's up, they open the
box expecting a letter. This works because you always do the two steps in that order:
letter in, *then* flag up.

Now imagine the letter and the flag are handled by two completely separate hands, and
those hands aren't required to move in the order you think, the flag hand could swing up
a beat before the letter hand actually finishes placing the letter. Your neighbor walks by
at exactly that beat, sees the flag up, opens the box... and finds yesterday's letter
still sitting there, because today's hasn't actually landed yet. Nothing about the flag
itself was broken, it went up exactly once, cleanly. The bug is that "flag up" became
visible to the neighbor *before* "letter placed" did, even though you did them in the
right order on your end.

That's this question, exactly. You're building the queue that sits between an audio
callback and the thread that writes to disk, or a market-data feed and the strategy
thread reading it. One thread pushes (places letters), one thread pops (checks the flag,
takes letters), and both must finish in a bounded number of steps: no lock, no
compare-exchange retry loop, no allocation. Just two indices and an array.

## What you're actually building

```cpp
static constexpr std::size_t capacity(); // = Capacity - 1 (one slot stays unused)
bool push(const T& v); // false if the ring is full
bool pop(T& out); // false if the ring is empty
```

`Capacity` is a power of two. `head_ == tail_` means empty; `(head_ + 1) & mask == tail_`
means full, that one unused slot is what keeps "empty" and "full" distinguishable from
each other.

## Where the naive version breaks

The naive version below is **algorithmically correct and still broken**. Every operation
is atomic, nothing tears, each index has exactly one writer, and yet the consumer can
read a slot it's been told is full and find the *previous* item still sitting in it. Your
job is to work out the minimum ordering that fixes it:

1. Which single load and which single store in `push`/`pop` need a stronger memory
 order, and which order each needs, and which accesses can stay `relaxed` forever,
 and why.
2. The producer writes `head_` on every push and the consumer writes `tail_` on every
 pop. As shipped, both indices sit on the same cache line. Fixing this isn't required
 to pass the tests, but measure it before and after, because the difference is the
 difference between a queue that scales and one that doesn't.

## Why the constraints exist

The tests run one producer and one consumer through 20,000 items × 12 trials with a
two-word payload (`seq`, `check = ~seq`). If the consumer is ever allowed to see a
published index before the element write that preceded it, the flag going up before the
letter lands, the payload comes out inconsistent and the test fails with the item
number.

**Pass = the harness returns CLEAN: correctness, ThreadSanitizer, and 150 shaken stress
runs.** Note what the harness will *not* do for you here: TSan models atomics as ordered
and cannot see a wrong memory order, only the payload consistency check can catch this
particular bug.
