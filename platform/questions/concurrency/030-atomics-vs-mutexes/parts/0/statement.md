# Atomic Operations vs Mutexes

## ELI5: a hand-clicker counter at a door

Picture the little hand-clicker a venue uses to count people walking through a door,
press the button, the number goes up by one. Several door staff could, in theory, share
one clicker: everyone just presses "+1" whenever someone walks past them. There's only
ever one number, and only ever one operation on it. That's simple enough that you don't
need a whole "wait your turn to touch the clicker" system, modern clickers (and modern
CPUs) can do "+1, safely, even if two people press at literally the same instant" without
anyone waiting in line at all.

That's the whole question: when the only thing being protected is a single number with a
single operation, do you really need the heavyweight "one person touches it at a time"
tool, or is there something lighter built for exactly this?

## What you're actually building

`HotCounter`, a counter that many threads increment concurrently and any thread can
read:

```cpp
class HotCounter {
public:
    HotCounter() noexcept;

    void increment() noexcept;
    long get() const noexcept;
};
```

## Requirements

1. `increment()` is safe to call from any number of threads at the same time.
2. After `T` threads each call `increment()` some number of times and all of them
 finish, `get()` returns the exact total. Not approximately, exactly, every run.
3. There is exactly one piece of state here (a number) and exactly one operation on it
 (add one). No second invariant to protect, no multi-step transaction to make atomic as
 a unit.

## Why the constraints exist

**Don't reach for the heaviest tool by default.** `<mutex>` and `<atomic>` are both
legal headers, the exercise is picking the right one for what requirement 3 actually
asks for, not just picking whichever one you reach for out of habit.
