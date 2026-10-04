`std::promise` gets you a future, but *you* have to call `set_value`/`set_exception` by
hand — which means catching every exception the callable might throw yourself, in a
`try`/`catch` around the call. There's a type built specifically to avoid writing that
boilerplate: it wraps a callable so that *invoking it* automatically stores the result, or
the exception, in the future for you. Which one?

---

`std::packaged_task<R()>` is that type. Build one by wrapping `f` and its arguments
together into a single no-argument callable (a lambda that closes over both, or
`std::bind`), call `.get_future()` on the task *before* you move it anywhere, then run the
task — invoking it, wherever it runs, is what fills in the future, exception or not.

---

`std::packaged_task` is move-only, and `std::thread`'s constructor takes its callable by
value/move — so `std::thread(std::move(task)).detach()` runs the task on a new thread with
no shared state left dangling in the caller's frame. `detach()` is safe here specifically
*because* the task owns everything it needs (the decay-copied callable and arguments) —
there's nothing left in the caller's stack for the task to reach into after `spawn_task`
returns, and the future is how the caller learns the task finished, not a join.
