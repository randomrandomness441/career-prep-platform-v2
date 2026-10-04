Two separate problems live in this exercise: getting a `std::packaged_task<R()>` (move-only)
into a queue that needs to hand tasks to workers as plain, uniform, callable objects; and
getting the workers to shut down without dropping whatever is still sitting in that queue.
Start with the first — what standard type erases "any callable that takes nothing and
returns nothing" into one uniform type, and how do you make a move-only `packaged_task` fit
inside something that wants to be copied?

---

`std::function<void()>` is the uniform type for the queue. `std::packaged_task` can't go
inside one directly (`std::function` requires its target to be copy-constructible,
`packaged_task` is move-only) — so wrap it in a `std::shared_ptr<std::packaged_task<R()>>`
and store a small lambda that captures the `shared_ptr` by value and calls `(*task)()`. The
lambda is copyable (copying a `shared_ptr` is cheap) even though the task inside it is not.
`submit()` builds the `packaged_task`, pulls its `future` out with `get_future()` *before*
moving it into the queue, and returns that future to the caller.

---

For shutdown: every worker sits in `cv_.wait(lk, [this]{ return stopping_ || !queue_.empty(); })`.
The bug that drops work is checking `stopping_` first: `if (stopping_) return;` exits before
ever looking at whether the queue still has something in it. Check the queue instead — `if
(queue_.empty()) return;` — and note that the wait's own predicate already guarantees the
queue can only be empty at that point if `stopping_` is also true, so this one flip is both
necessary and sufficient. The destructor's job is just to set `stopping_`, `notify_all()`,
and `join()` every worker; it never has to know the queue was non-empty, because the workers
themselves keep draining it until it isn't.
