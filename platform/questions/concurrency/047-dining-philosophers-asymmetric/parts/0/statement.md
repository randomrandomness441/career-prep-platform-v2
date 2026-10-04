# Dining Philosophers with Asymmetric Resource Starvation

## ELI5: the one person at the table who never stops eating

Same five philosophers, five chopsticks table as [[021-dining-philosophers]], but now
one of them is *ravenous*. The instant they finish a bite, they're already reaching for
chopsticks again, no pause. Their four neighbors eat normally: finish, sit back, think
for a while, and only then reach for chopsticks again.

The question isn't "does this deadlock", 021 already solved that. It's: does the
non-stop philosopher's greediness let them hog the table, eating thousands of times more
than everyone else because they're *always* first in line for a fork the moment it's
free? Or does the table stay reasonably fair even when one guest never lets up?

## What you're actually building

```cpp
class DiningPhilosophers {
public:
    DiningPhilosophers();
    void wantsToEat(int philosopher,
    std::function<void()> pickLeftFork,
    std::function<void()> pickRightFork,
    std::function<void()> eat,
    std::function<void()> putLeftFork,
    std::function<void()> putRightFork);
};
```

## Requirements

1. Everything 021 required still holds: no two neighbors ever share a fork at the same
 time, and no call ever hangs.
2. **The hungry philosopher asking more often should give it *some* edge, not an
 unbounded one.** Under sustained load where one philosopher never waits between meals
 and its neighbors wait a realistic amount, the hungry philosopher's meal count must
 stay within a bounded multiple of its neighbors' average, not run away to thousands of
 times more.

## Why the constraints exist

**Fix this without changing the fork-acquisition order that 021 already established
prevents deadlock**, that part of the design isn't what's being asked about here. This
question is purely about fairness under asymmetric demand, layered on top of a
deadlock-free foundation you're not supposed to touch.
