# Follow-up 2, scale it to 100 runners

## ELI5: the relay race, but with a stadium full of runners

Same idea as before, runner 1 goes, then runner 2, then runner 3, and so on, strictly
in order, except now there are **100 runners**, not three, all lined up around the
track, each told "go!" in some random order by 100 different starters.

Your part-0 solution used one "gate" (a semaphore) between each pair of runners: a gate
between 1 and 2, another between 2 and 3, and so on. That's 99 gates for 100 runners.
Workable, but clunky, and if the race organizer adds a 101st runner next season, you're
back in here building gate #100 by hand.

## What you're actually building

```cpp
class Ordered {
public:
    explicit Ordered(int n);
    void go(int id, std::function<void()> print); // id is 1..n
};
```

`go(id, print)` must call `print()` only after every smaller id has already finished
printing. Threads call `go` in whatever order the scheduler picks.

## Your task

Solve it with **one mutex and one condition variable** instead of a chain of gates,
just a single shared number that says "whose turn is it right now," and everyone waits
on the same condition variable until that number is theirs.

**Then answer in your Notes** (this is the part an interviewer actually cares about):

> With 100 threads waiting on one condition variable, `notify_all()` wakes all 99 losers
> up just to re-check "is it my turn?" and find out it isn't, for every single wakeup but
> one. What's that pattern called, and in what situation is the 99-gate version from
> part 0 actually the *better* engineering choice?
