`std::thread` copies every argument into storage the new thread owns, because the thread
may outlive the caller. Ask of each version: **is the thread touching the caller's object,
or a copy of it?**
---
For the lambda: `[c, n]` copies `c` into the closure. The `mutable` keyword was only needed
because you were writing to that copy. Change what the capture list says.
---
For the function form, `std::thread t(add_n, c, n)` will not compile — arguments are
decay-copied, and a copy cannot bind to `Counter&`. You must state that you meant a
reference:

```cpp
std::thread t(add_n, std::ref(c), n);
```

`std::ref` is in `<functional>`.
