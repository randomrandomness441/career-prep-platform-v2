The release must happen whether or not the callback threw. What C++ construct guarantees
a piece of code runs on both the normal and the exceptional path?
---
Two ways.

Explicit: `try { printFirst(); } catch (...) { gate2.release(); throw; } gate2.release();`

RAII: an object whose destructor releases the gate. Destructors run during stack
unwinding, so it happens automatically.
---
The RAII version is what you would write in production, because it cannot be forgotten
and stays correct if someone adds an early `return` later:

```cpp
struct Opener {
    std::binary_semaphore& s;
    ~Opener() { s.release(); }
};
void first(std::function<void()> p) {
    Opener o{gate2};   // releases on the way out, however we leave
    p();
}
```
