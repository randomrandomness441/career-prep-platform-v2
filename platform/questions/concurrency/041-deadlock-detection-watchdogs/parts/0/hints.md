Calling `op()` directly, on the caller's own thread, can never satisfy the deadline
requirement — if `op` hangs, so does `run_with_deadline`, because they're the same thread.
`op` has to run somewhere you can walk away from independently of whether it ever finishes.

---

You need a way to learn "op finished" from another thread without blocking until it
actually does. `std::promise`/`std::future` gives you exactly that: fulfil the promise from
the worker thread when `op()` returns, and call `future.wait_for(deadline)` from the caller
— it returns as soon as either the promise is fulfilled or the deadline elapses, whichever
comes first, and tells you which one happened (`std::future_status::ready` vs
`std::future_status::timeout`).

---

Don't use `std::async` to launch `op`. `std::async(std::launch::async, ...)`'s returned
future has a special rule: if nobody calls `.get()` on it, its destructor **blocks until the
task finishes** — which defeats the entire point of walking away from a stuck operation. A
plain `std::thread` has no such rule; if the deadline wins, `.detach()` it and return `false`
— that thread (and anything it's holding) leaks for the life of the process if `op` really
never returns, which is the honest cost of not being able to force-stop it.
