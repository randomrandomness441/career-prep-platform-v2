You cannot make the scheduler alternate for you. Add one piece of shared state that says
whose turn it is — a single `bool foo_turn` — and make the thread whose turn it is *not*
go to sleep until the flag changes.
---
`std::condition_variable` is the sleep-until-a-flag-changes tool. It always comes with a
mutex, because the flag it waits on is shared state and must be read under the lock.

```cpp
std::unique_lock<std::mutex> lk(m);
cv.wait(lk, [this]{ return foo_turn; });   // sleeps; re-checks foo_turn on every wake
```

Use the two-argument form. The one-argument `cv.wait(lk)` means "wake me when someone
notifies", which is not the same as "wake me when it is my turn".
---
```cpp
void foo(std::function<void()> printFoo) {
    for (int i = 0; i < n; ++i) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [this]{ return foo_turn; });
        printFoo();
        foo_turn = false;
        lk.unlock();
        cv.notify_one();
    }
}
```
`bar()` is the mirror image: wait for `!foo_turn`, print, set `foo_turn = true`, notify.

`notify_one` is enough because there is only ever one other thread that could be waiting.
Unlock before you notify, or the thread you just woke immediately blocks on the mutex you
are still holding.
