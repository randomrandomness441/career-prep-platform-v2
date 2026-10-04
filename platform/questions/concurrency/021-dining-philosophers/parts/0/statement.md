# The Dining Philosophers

## ELI5: five people, five chopsticks, a table that can freeze solid

Five philosophers sit around a round table. Between each neighboring pair sits a single
chopstick, five chopsticks total for five people. Eating needs **two** chopsticks, one
from each side, and a chopstick can only be in one hand at a time.

Here's the disaster scenario: everyone reaches for the chopstick on their *left* at the
same moment. Now every single philosopher is holding exactly one chopstick, and every
single one of them is waiting for their right-hand neighbor to put theirs down, which
will never happen, because that neighbor is doing the exact same thing. Five people, all
holding one chopstick each, all waiting forever, nobody ever eats again. That frozen table
is a deadlock, and it's the whole reason this puzzle exists.

## What you're actually building

```cpp
class DiningPhilosophers {
public:
    DiningPhilosophers();

    void wantsToEat(int philosopher, // 0..4
    std::function<void()> pickLeftFork,
    std::function<void()> pickRightFork,
    std::function<void()> eat,
    std::function<void()> putLeftFork,
    std::function<void()> putRightFork);
};
```

Philosopher `i`'s **left** fork is fork `i`; their **right** fork is fork `(i + 4) % 5`.
So philosophers `i` and `i+1` compete for fork `i+1`, and every philosopher shares one
fork with each neighbor.

`wantsToEat` is called concurrently, many times, from five threads, one per philosopher.
Each call is one complete meal.

## Requirements

1. Call `eat()` exactly once per call, and only while holding both of that philosopher's
 forks, that is, between the two `pick` callbacks and the two `put` callbacks.
2. `pickLeftFork` and `pickRightFork` must both be called before `eat`, and
 `putLeftFork`/`putRightFork` both after it. The order *within* each pair is yours to
 choose, and choosing it well is most of the exercise.
3. **Every call must return.** No philosopher may be left holding a fork and waiting
 forever, no frozen table.
4. Two philosophers who aren't neighbors share no fork, so they must be able to eat at
 the same time. A single global mutex around the whole function satisfies requirements
 1–3 and is *not* an acceptable answer, the tests check that concurrent eating
 actually happens, not just that nothing breaks.

## Why the constraints exist

- **One `std::mutex` per fork.** That's the model of the problem: a fork is a resource
 with an owner, and the mutex *is* the ownership.
- **No sleeping, no polling, no `try_lock` retry loop with a backoff.** The fix here is
 structural, it changes *how* forks get picked up, not how hard you retry when it goes
 wrong.

The boilerplate is the version everyone writes: take the left fork, then take the right
fork. Run the tests on it before you change anything, the frozen table is worth seeing
for yourself.
