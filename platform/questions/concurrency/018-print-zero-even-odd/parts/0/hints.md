Three threads, so three places a thread can be asleep. Give each thread its own gate rather
than making two of them queue at the same one — otherwise nothing in your code decides
*which* of the two number threads wakes up, and "whoever the runtime picks" is not an
ordering.
---
Start `zero`'s gate open and the other two shut, so the sequence begins with a `0`
regardless of which thread the OS schedules first:

```cpp
std::binary_semaphore sem_zero{1}, sem_even{0}, sem_odd{0};
```

`zero()` is the router. On iteration `i` it has just printed the `0` that precedes the
number `i`, so `i`'s parity tells it which gate to open. `even()` and `odd()` each open
`sem_zero` again when they are done.
---
```cpp
void zero(std::function<void(int)> printNumber) {
    for (int i = 1; i <= n; ++i) {
        sem_zero.acquire();
        printNumber(0);
        if (i % 2 == 1) sem_odd.release();
        else            sem_even.release();
    }
}

void odd(std::function<void(int)> printNumber) {
    for (int i = 1; i <= n; i += 2) {
        sem_odd.acquire();
        printNumber(i);
        sem_zero.release();
    }
}
```
`even()` is the same shape with `i = 2; i += 2`.

Look at what just happened to `sem_zero`: the `zero` thread acquired it, and the `odd` and
`even` threads release it. That is the whole reason this is a semaphore and not a mutex — a
`std::mutex` must be unlocked by the thread that locked it.
