# A Lock Hierarchy That Catches Deadlock

## ELI5: a building where you can only go down

Imagine a tall building where every floor is numbered, and there's one strict rule: once
you step onto a floor, you're only allowed to go *down* from here, to a lower-numbered
floor, never back up, and never sideways to another floor at the same number. If you
try, an alarm goes off immediately.

This sounds annoying, but it's actually a clean way to make deadlocks *impossible*: two
people can never end up each waiting for the other to leave a floor, because everyone in
the whole building is always moving in the same direction (down). The classic two-mutex
deadlock from [[005-scoped-lock]], thread 1 holds floor A waiting for floor B, thread 2
holds floor B waiting for floor A, can't happen if "floor B" is always numbered lower
than "floor A" for everyone, every time.

## What you're actually building

`hierarchical_mutex`: a lock that knows what floor it lives on, and refuses to be locked
out of order.

```cpp
class hierarchical_mutex {
public:
    explicit hierarchical_mutex(unsigned long value);

    hierarchical_mutex(const hierarchical_mutex&) = delete;
    hierarchical_mutex& operator=(const hierarchical_mutex&) = delete;

    void lock(); // throws std::logic_error on a hierarchy violation
    void unlock();
    bool try_lock(); // same check; returns false only if the mutex is busy
};
```

**The rule.** Every thread carries a *current floor*, starting at `ULONG_MAX`, "not in
the building, no floor at all." `lock()` only succeeds if this mutex's floor number is
**strictly lower** than the thread's current floor. On success, the mutex remembers what
floor the thread was on before, and moves the thread down to its own floor. `unlock()`
carries the thread back up to whatever floor it remembers.

So a thread can walk 10000 → 5000 → 1000 and back up again, floor by floor, but the
moment it tries 1000 → 5000 (going back up) or 5000 → 5000 (sideways), the alarm
(`std::logic_error`) goes off instead of the lock succeeding.

## Requirements

1. **The current floor is per thread.** Two threads walking the building at the same
 time must not see or affect each other's floor number.
2. `lock()` throws `std::logic_error` on a violation, and throws it **before** it blocks
 , you get the alarm instantly, you don't wait around first.
3. A rejected lock changes nothing, the thread still holds exactly what it held before
 trying.
4. Equal floor numbers count as a violation too, the comparison is strictly less-than,
 not less-than-or-equal.
5. It must satisfy *Lockable*, `lock()`, `unlock()`, `try_lock()`, so
 `std::lock_guard<hierarchical_mutex>` and `std::unique_lock<hierarchical_mutex>` work
 on it unchanged, same as any other mutex.
6. It's still a real mutex underneath. Mutual exclusion has to actually happen, not just
 the floor-number bookkeeping.

## Why the constraints exist

- **Build on `std::mutex`.** The floor-number check is bookkeeping wrapped around a real
 mutex, not a replacement for one.
- **Where you save the previous floor matters.** Think about a thread holding three of
 these at once and unlocking them in reverse order, each `unlock()` needs to restore
 the *right* previous floor, not just "whatever the last one was globally."
