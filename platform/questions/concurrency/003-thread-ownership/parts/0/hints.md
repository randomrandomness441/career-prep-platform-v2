Start with the destructor: `if (t.joinable()) t.join();`. The `joinable()` check matters —
joining an empty handle throws. An object that has been moved from holds an empty handle.
---
For move-assignment, ask what `std::thread::operator=` does when the left side already owns
a running thread. It does not join it for you. It calls `std::terminate()`.

So before taking over, you must deal with the thread you already have.
---
```cpp
joining_thread& operator=(joining_thread&& other) noexcept {
    if (this != &other) {          // self-move guard
        if (t.joinable()) t.join();   // finish what we already own
        t = std::move(other.t);
    }
    return *this;
}
```
The constructor just needs to hand the callable straight to `std::thread`:

```cpp
explicit joining_thread(std::function<void()> f) : t(std::move(f)) {}
```
`explicit` plus the deleted copy constructor keeps it from being used where you don't mean it.
