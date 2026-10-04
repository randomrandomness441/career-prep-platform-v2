# Testing Concurrent Code

## ELI5: jiggling the door handle instead of signing a waiting list

A single-occupancy bathroom has a lock, but no sign-up sheet outside. If it's occupied,
you don't go sit down and wait to be called, you just keep jiggling the handle every
split second until it turns. That's a spinlock: instead of the OS actually putting a
waiting thread to sleep (a real waiting list), the thread just keeps trying, over and
over, burning CPU the whole time, until it succeeds.

## What you're actually building

```cpp
class Spinlock {
public:
    void lock(); // blocks (busy-waiting) until it can be acquired
    void unlock();
};
```

## Requirements

1. **Mutual exclusion**: between a successful `lock()` and the matching `unlock()`, no
 other thread's `lock()` may return, only one person in the bathroom at a time.
2. **`lock()` busy-waits (spins)**, no OS-level blocking primitive, no `std::mutex`
 underneath. Jiggling the handle, not signing a list.
3. Correct under real contention from many threads.

This question is as much about *how this gets verified* as about the three lines of code
that make it correct, read the reading before and after attempting it.
