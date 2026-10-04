You cannot make a thread start earlier. You can only make the other two **wait**.
So: what must `second()` wait for, and who tells it the wait is over?
---
Two gates, both closed at the start. `second()` waits at gate 1, `third()` waits at gate 2.
`first()` opens gate 1 when it finishes; `second()` opens gate 2 when it finishes.
---
`std::binary_semaphore` is exactly that gate. Construct with `{0}` for closed.
`acquire()` waits at the gate, `release()` opens it.

```cpp
std::binary_semaphore gate2{0}, gate3{0};
```
Note that a semaphore can be released by a *different* thread than the one that acquires
it. A mutex could not do this job.
