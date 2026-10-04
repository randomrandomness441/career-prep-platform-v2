# A Thread Pool with future-Returning submit()

## ELI5: hiring a new contractor for every tiny errand

`std::async` is like hiring a brand-new contractor for every single task, no matter how
small, even "hang this one picture." For a handful of big jobs, that's fine. The hiring
overhead is nothing next to the job itself. But for thousands of tiny errands, the hiring
process itself, interviewing, paperwork, showing up, packing up again, becomes the actual
bottleneck. You spend more time hiring than working.

The fix: keep a standing crew on-site instead. Hire a fixed team once, at the start, and
hand them every task as it comes in. No hiring paperwork per errand, just "here's your next
job" to someone already there and ready.

## What you're actually building

Build `ThreadPool`: a fixed number of worker threads, created once, that pull work from one
shared queue for as long as the pool lives.

```cpp
class ThreadPool {
public:
    explicit ThreadPool(std::size_t n_threads);
    ~ThreadPool();

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;
};
```

## Requirements

1. `submit` accepts any callable plus its arguments (like `std::thread`'s constructor) and
   returns a `std::future` for its result. The caller `.get()`s it exactly like any other
   future. They shouldn't be able to tell the task ran on a pooled worker rather than its
   own thread, except that it was faster to start.
2. An exception thrown inside a submitted task comes back out through that task's `future`
   when the caller calls `.get()`, not out of `submit()`, not as a crash.
3. **Shutdown is graceful.** When the pool is destroyed, every task that was already
   queued, whether or not any worker had picked it up yet, runs to completion before the
   destructor returns. No submitted task is ever silently dropped. The destructor still
   returns promptly (it does not wait for tasks nobody has submitted yet, only for the ones
   already in the queue at the moment of destruction) and joins every worker thread cleanly.
4. The pool is neither copyable nor assignable.

## Why the constraints exist

- **Do not spawn a new `std::thread` per `submit()` call.** That's `std::async` with extra
  steps, hiring a contractor per errand all over again. The worker threads are created once,
  in the constructor, and live until the destructor joins them.
- **The queue holds type-erased work.** Figure out how to put a `std::packaged_task`, which
  is move-only, into a container that generally wants copyable things.
