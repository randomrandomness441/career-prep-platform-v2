Stop thinking "gate per thread" and start thinking "one shared number saying whose turn
it is". Each thread sleeps until that number equals its own id.
---
```cpp
std::mutex m;
std::condition_variable cv;
int turn = 1;
```
`cv.wait(lk, pred)` unlocks the mutex and sleeps while `pred` is false, re-locking and
re-checking each time it wakes. Your predicate is `turn == id`.
---
```cpp
void go(int id, std::function<void()> print) {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [&]{ return turn == id; });
    print();
    ++turn;
    cv.notify_all();
}
```
`notify_all`, not `notify_one` — `notify_one` may wake a thread whose id is not the new
turn, and that thread goes straight back to sleep without passing the signal on. Everyone
else sleeps forever. This is the classic lost-wakeup bug.
