Why does the flag version hang, even though it calls `notify_all()` before joining?
Because waking a sleeping thread is not the same as giving it permission to leave the
wait: the wait's predicate decides whether to sleep again. The predicate says "queue
non-empty?" — still false — so the worker rolls over. The stop has to be part of the
condition the waiter is waiting on.
---
`std::jthread` hands your thread function a `std::stop_token` if it takes one as its
first parameter. `request_stop()` flips it, from any thread; the destructor calls
`request_stop()` and then `join()` for you. The missing link is the parked wait:
which wait overload understands a stop token, and on which condition variable type?
(`std::condition_variable_any` — a plain `std::condition_variable` has no such overload
and will not compile it.)
---
The run loop is the whole solution:

```cpp
void run(std::stop_token st) {
    std::unique_lock<std::mutex> lk(m_);
    while (cv_.wait(lk, st, [this] { return !jobs_.empty(); })) {
        // drain the queue; loop back to wait
    }
}   // wait returned false: stop requested with an empty queue -> exit
```

Declare the `jthread` member LAST so it is destroyed FIRST: its stop+join then runs
while the mutex, cv and queue still exist. Getting this backwards is a use-after-free
that only appears at shutdown.
