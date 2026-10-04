The bug is that "is it built?" (a read) and "build it" (a write) are two separate steps,
and any number of other threads can be between them. Whatever replaces it must make the
check-and-act atomic *for you*. Since C++11 the compiler does exactly that for one
specific construct — a static local variable. Which line of `get()` could hold it?
---
```cpp
static T& get() {
    static T instance;   // the runtime guarantees: one construction, concurrent
    return instance;     // callers wait, constructor writes visible after the wait
}
```
This is the Meyers singleton. The guarantee comes from the standard's rules for static
local variables, and every implementation ships it: clang/GCC via `__cxa_guard_acquire`,
MSVC via a guard variable of its own. No mutex of yours appears anywhere.
---
If you cannot use a static local — the instance type is fixed elsewhere, or the "once"
action isn't a construction — the same guarantee is available manually:
`std::once_flag` + `std::call_once`. Know both; the interview answer is the static
local, with call_once as the escape hatch. Check requirement 4 too: a constructor
exception must leave the initialization *not done*, so the next call retries — verify
your version does that rather than caching the failure.
