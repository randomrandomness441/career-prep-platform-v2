There is one sequence, so there is one piece of shared state: `int current`, starting at 1,
guarded by one mutex. Every thread waits for `current` to be a value it owns, prints,
increments, and wakes the others. Each thread's predicate is just its FizzBuzz rule applied
to `current`.
---
Two things the loop must get right.

**Termination.** When `current` passes `n` every thread has to wake up and return, or the
caller's `join()` hangs. So the wait predicate is *"my value, or we're finished"*:

```cpp
cv.wait(lk, [&]{ return current > n || (current % 3 == 0 && current % 5 != 0); });
if (current > n) return;
```

**Who to wake.** After `++current` you must wake the thread that owns the new value — but
you have no way to address it. Ask what `notify_one` does when the thread it picks is not
that one.
---
```cpp
void fizz(std::function<void()> printFizz) {
    while (true) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [this]{ return current > n || (current % 3 == 0 && current % 5 != 0); });
        if (current > n) return;
        printFizz();
        ++current;
        lk.unlock();
        cv.notify_all();      // notify_one deadlocks — see the reading
    }
}
```
The other three are identical with a different predicate and a different print.

`notify_all` is required because the four sleepers are *not* interchangeable: they wait on
four different conditions, and `notify_one` may wake one whose condition is still false. It
rechecks, sleeps again, and the notification is gone — nobody else was told. The price of
`notify_all` is three wasted wakeups per number. You avoid both by giving each thread its
own `condition_variable` and notifying exactly the right one.
