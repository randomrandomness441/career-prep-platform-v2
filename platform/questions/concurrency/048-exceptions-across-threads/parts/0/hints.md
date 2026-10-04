Start from the shape you'd guess: make a `std::promise<R>`, hand its `std::future` to the
caller, and run a lambda on a `std::thread` that calls `f()` and does
`p.set_value(f())`. That gets requirements 1, 2 and 4. It does not get requirement 3 — if
`f()` throws inside that lambda, the exception propagates out of the thread's entry
function, and `std::terminate()` runs before `set_value` or anything else has a chance to.
---
`std::promise` has a second setter for exactly this: `set_exception(std::exception_ptr)`.
Wrap the body in `try { ... } catch (...) { p.set_exception(std::current_exception()); }`.
`std::current_exception()` captures whatever is being handled right now into a portable
`std::exception_ptr`, valid even after the `catch` block ends — that's what makes it safe to
stash inside the promise and let another thread pull it back out later.
---
The promise has to outlive the `run_task` call — it needs to survive until the worker thread
finishes, which may be long after `run_task` has returned. A `std::shared_ptr<std::promise<R>>`
captured by value into the thread's lambda is the simplest way to keep it alive exactly that
long:

```cpp
template <typename F>
std::future<std::invoke_result_t<F>> run_task(F f) {
    using R = std::invoke_result_t<F>;
    auto p = std::make_shared<std::promise<R>>();
    std::future<R> fut = p->get_future();
    std::thread([p, f = std::move(f)]() mutable {
        try {
            if constexpr (std::is_void_v<R>) { f(); p->set_value(); }
            else                              { p->set_value(f()); }
        } catch (...) {
            p->set_exception(std::current_exception());
        }
    }).detach();
    return fut;
}
```
`if constexpr` is needed because `set_value()` (no argument) and `set_value(x)` are
different calls — `R = void` can't be handled by one code path without it.
