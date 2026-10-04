# The Thundering Herd Problem

## ELI5: one parking spot opens, and 50 cars all lunge for it

A tiny parking garage has, say, 3 spots, and 50 cars are circling, waiting for one to
open up. A car finally leaves. The garage's PA system blares "A SPOT IS OPEN!" to
everyone at once. All 50 cars immediately gun it toward that one spot, one of them gets
it, and the other 49 all just burned gas, made noise, and have to go right back to
circling, having accomplished nothing. That stampede-for-one-spot is the "thundering
herd": correct in the end (the spot does get taken by *someone*), but wildly wasteful
every single time it happens.

What you actually want: when one spot opens, wake up *roughly* the right number of cars
, ideally just enough that exactly the ones who can now get a spot do, without needlessly
startling the other 47 who never had a chance this round.

## What you're actually building

A fixed-size resource pool, think a database connection pool with a hard limit on
connections. Callers borrow a slot, use it, and give it back.

```cpp
class SlotPool {
public:
    explicit SlotPool(int slots);
    void acquire(); // blocks until a slot is free, then takes one
    void release(); // returns a slot to the pool

    std::atomic<long> wasted_wakeups{0}; // instrumentation, see below
    std::atomic<long> started{0}; // instrumentation, see below
};
```

## Requirements

1. `acquire()` blocks (no busy-waiting) until a slot is available, then takes it.
2. `release()` returns a slot. It must not require the caller to say which slot, a plain
 counter of free slots is enough.
3. At any moment, the number of threads that have called `acquire()` and not yet called
 `release()` never exceeds the pool's size.
4. **When one slot becomes free and many threads are waiting, only the threads that can
 actually proceed should ever be woken.** Waking every waiter to let one of them
 through isn't a correctness bug, every design here is required to be correct
 regardless, but it's the specific inefficiency (the stampede) this question is about,
 and it's graded.

## Required instrumentation (part of the interface, not incidental)

- `started`, increment once, at the very top of `acquire()`, before anything else. This
 counts every car that joined the circling line.
- `wasted_wakeups`, every time a thread inside `acquire()` wakes from waiting on the
 condition variable, rechecks its predicate, *and finds it still false*, increment this
 once. A thread that wakes and immediately succeeds doesn't count, this is purely the
 count of cars that gunned it and found the spot already taken.

## Why the constraints exist

- **One mutex, one condition variable. No `try_lock` polling loop.** The fix here is
 about *who* you wake, not about spinning harder.
- **Every waiter is interchangeable**, nothing distinguishes which thread "should" get
 the next free slot; any one of them taking it is correct. There's no queue-position
 fairness requirement to worry about on top of the wakeup problem.
