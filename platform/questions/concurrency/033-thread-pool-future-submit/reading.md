## 1. Reframe the problem

`std::async` answered "how do I run this on another thread and get the result back" for the
quicksort question. It did that by creating a real OS thread per call. That's the right
tool when you have a few, coarse tasks, a handful of recursive sort partitions. It is the
wrong tool the moment you have *many, small* tasks, because thread creation is not free:
every `std::async(std::launch::async, ...)` pays the OS for a new stack, a new kernel
scheduling entity, and eventual teardown, whether the task inside takes 5 seconds or 5
microseconds.

The fix threads (pun intended) through this whole course as a pattern: stop creating a
resource per unit of work, and instead create a fixed number of them once, then hand work to
whichever one is free. You already built exactly this shape for a queue of *data*, the
bounded blocking queue. A thread pool is the same idea for *work*: a fixed set of long-lived
worker threads, and a shared, mutex-protected queue between "submit this" and "someone runs
it."

The part that's actually new here is what a worker takes *out* of that queue. A data queue
moves values. A task queue has to move work that hasn't necessarily produced its type yet,
`submit(f, args...)` might return an `int`, a `std::string`, or nothing, and every one of
those needs to become a *uniform* thing the queue can hold and a worker can just... call. And
the caller still needs to get their specific result back out, correctly typed, through a
`std::future`. That's two type-erasure problems stacked on top of the producer-consumer
problem you already know how to solve.

## 2. The tools

### Type-erasing `packaged_task` into the queue

`std::packaged_task<R()>` is the machine that already does "run this callable, and put
whatever it returns (or throws) into a `std::future` for someone else to read", you built
one by hand for `spawn_task` in the `std::async` question, and `packaged_task` is the
library's version of it:

```cpp
std::packaged_task<int()> pt([]{ return 42; });
std::future<int> fut = pt.get_future(); // pull the future out BEFORE running or moving pt
pt(); // runs the callable, stores the result in fut's state
```

The queue, though, needs to hold tasks of *every* return type in one container. The uniform
answer is `std::function<void()>`, but `packaged_task` is move-only (its `future`'s shared
state can only have one owner) and `std::function` needs its target to be copy-constructible.
Wrap it:

```cpp
auto task = std::make_shared<std::packaged_task<R()>>(std::move(callable));
std::function<void()> erased = [task] { (*task)(); }; // copying the lambda copies a shared_ptr
```

Copying `erased` is cheap (one refcount bump), and there is still exactly one
`packaged_task` underneath, wherever the last copy of the `shared_ptr` ends up.

### `std::invoke_result_t`, deducing the return type of "anything, called with anything"

```cpp
template <typename F, typename... Args>
auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using R = std::invoke_result_t<F, Args...>;
    ...
}
```

`std::invoke_result_t<F, Args...>` asks the compiler "if you called an `F` with these
`Args...`, what type would you get back" without actually calling it, it's the type-level
version of `std::invoke`. That's what lets `submit` return the *right* `std::future<R>` for
whatever was handed to it, the same way `std::thread`'s constructor accepts any callable plus
arguments.

### The queue: one mutex, one condition variable, N workers instead of 1 consumer

This is the bounded queue's shape with the bound removed and the consumer side turned into a
loop that runs forever:

```cpp
std::mutex m_;
std::condition_variable cv_;
std::queue<std::function<void()>> queue_;
bool stopping_ = false;

void worker_loop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(m_);
            cv_.wait(lk, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) return; // see section 3 for why this line, not `if (stopping_)`
            task = std::move(queue_.front());
            queue_.pop();
        }
        task(); // run it OUTSIDE the lock -- see section 6
    }
}
```

`submit` locks, pushes the type-erased task, unlocks, and calls `notify_one()`, one waiting
worker wakes, exactly like one consumer waking in the bounded queue.

### Shutdown: a flag the destructor sets, that the workers themselves have to respect correctly

```cpp
~ThreadPool() {
    { std::lock_guard<std::mutex> lk(m_); stopping_ = true; }
    cv_.notify_all(); // everyone parked in wait() needs to hear this, not just one
    for (auto& t : workers_) t.join();
}
```

`notify_all`, not `notify_one`, every worker might be asleep in `wait()`, and all of them
need to wake up and re-check, not just one. This is a case where waking N-1 threads that then
find nothing to do (a thundering herd, in the terms of that other question) is exactly
correct: it happens once, at shutdown, not on every enqueue, so the cost is irrelevant.

## 3. The broken version, first

Everything above is identical between the naive version and the correct one, the
constructor, `submit`, the queue, the condition variable. The only difference is one line in
`worker_loop`:

```cpp
cv_.wait(lk, [this] { return stopping_ || !queue_.empty(); });
if (stopping_) return; // <-- checks the wrong thing first
task = std::move(queue_.front());
```

**Why this looks right.** `stopping_` means "the pool is shutting down." Checking it first
and returning reads as the obviously correct way to make a worker stop. The predicate that
woke this thread up was `stopping_ || !queue_.empty()`, if you don't stop and think about
*which* disjunct is true, "we were woken because we're stopping, so stop" is the natural
sentence to write. It compiles, it passes every test that doesn't specifically try to catch a
task in the queue at shutdown time, and a pool that's only ever destroyed after its queue has
naturally drained (the common case in a quick manual test) never reveals the bug at all.

**What actually happens.** Pin two workers inside a blocking task each (so neither is free to
pull from the queue), submit 40 more lightweight tasks behind them, they can only be sitting
in the queue, since both workers are provably busy, then destroy the pool while the workers
are still pinned. Real output, running the boilerplate's own class:

```
trial 0: 0 of 40 queued tasks ran before shutdown finished (and 2 blockers had started) --
queued work was dropped
```

Zero of forty. Not "most of them", every single one, every trial. `stopping_` becomes true
while the two workers are still inside their blocking tasks; the moment each one finishes and
loops back around, the predicate is already satisfied by `stopping_` alone, so `if
(stopping_) return;` fires before either worker so much as looks at the queue. The 40
`packaged_task`s are destroyed, unrun, still sitting in `queue_`, when the destructor's
`std::queue` member is torn down.

And that isn't silent to whoever's holding the futures, it's silent to the *pool*, but loud
to the caller. Querying one of those dropped tasks' futures afterward:

```
f.get() threw future_error: The associated promise has been destructed prior to the
associated state becoming ready.
```

A caller who submitted work right before a shutdown they didn't even know was happening gets
a `future_error` instead of their result, with no indication anywhere in the pool's own code
that anything was dropped. That is strictly worse than a crash: a crash gets noticed.

**The fix** inverts which condition gets checked first: `if (queue_.empty()) return;`. The
wait's predicate already guarantees that if the queue is empty, `stopping_` must be true (that
disjunction is the *only* way to stop waiting), so this one line is both the necessary and the
sufficient condition to exit, and unlike the naive version, it can never fire while there is
still work sitting there. The general lesson: when a stop flag and a work queue are both part
of one wait condition, decide "is there still work" before you decide "are we stopping",
`stopping_` tells a worker it may leave once idle, not that it must leave now.

## 4. Real-world usage

**Why thread pools exist.** The cost that motivates every fixed-size pool is thread creation
and teardown, measured on this machine below at roughly **17.8 µs** for create-plus-join of
an empty `std::thread`. Any workload with tasks shorter than that, submitted more often than
that, loses to a pool that reuses threads no matter how the tasks themselves are optimized.
This is the same argument the 003 reading makes for `joining_thread`/`jthread` pools, this
question is that argument, built out into the actual reusable structure.

**Where you meet it:**

- **HTTP/RPC servers.** A fixed pool of workers, one task per request. The queue is exactly
 the backpressure mechanism from the bounded-queue question, a server under load queues
 requests instead of spawning a thread per connection, which is how you avoid a thread-bomb
 under a traffic spike.
- **Any framework with an "executor" or "dispatch queue" concept**, Java's
 `ExecutorService`, .NET's `ThreadPool`, Apple's Grand Central Dispatch, Boost.Asio's
 `io_context` with a thread pool behind it. All of them are "fixed workers, shared task
 queue, future-like handle for the result," this class with a different API surface.
 Anthony Williams' own book builds essentially this class in chapter 9 as the canonical
 answer to "I have a lot of independent, short-lived tasks."
- **Parallel algorithms with many fine-grained tasks.** Where the quicksort question spawned
 a handful of coarse `std::async` tasks and could afford thread-per-task, an algorithm that
 wants to fan out into thousands of small chunks (a parallel `for_each` over a large range,
 say) needs a pool, thread-per-chunk would spend more time on `pthread_create` than on the
 work itself.

**Where NOT to use it:**

- **Blocking I/O inside a fixed-size CPU pool.** A worker blocked on a socket read or a disk
 write is a worker that isn't running CPU-bound tasks. Size the pool for I/O concurrency
 (much larger than core count, or use a separate I/O-oriented pool / async I/O) rather than
 starving your CPU-bound work behind blocked workers.
- **Tasks that submit and wait on other tasks from inside the same pool.** Section 6 has the
 measured deadlock. If task A, running on a worker, submits task B and blocks on B's future,
 and every worker is doing the same thing, B never runs. Either don't nest, or use a pool
 that lets a blocked worker help drain the queue while it waits (the work-stealing design in
 a later question).
- **A single huge, rare task.** Reserving a whole worker for something that runs once an hour
 wastes a thread that could be serving the steady stream of small work. Give rare, heavy
 jobs their own dedicated thread or their own pool instead of mixing them into one meant for
 high-frequency small tasks.
- **As a way to get more parallelism than you have cores.** More workers than
 `hardware_concurrency()` doesn't create parallelism, it creates context-switch overhead for
 CPU-bound tasks, size the pool to the workload's actual nature (I/O-bound: oversubscribe;
 CPU-bound: match core count), not to "however many tasks I have in flight."

## 5. Performance

Measured on this machine, Apple M5, 10 cores, `-O2`, min of two runs per number.

**Pool vs. thread-per-task**, one task each, round-tripped through `submit()`/`get()`
sequentially, 200,000 tasks that just return an `int`:

| | ns/task |
|---|---|
| pool(1 worker) | ~5,400 |
| pool(2 workers) | ~5,050 |
| pool(4 workers) | ~5,100 |
| pool(8 workers) | ~5,300 |
| raw `std::thread` per task, create+join | **~17,800** |

Sequential submit-then-immediately-`get()` pays a full sleep/wake round trip no matter how
many workers exist, the caller is blocked waiting on exactly one task the whole time, so
extra workers can't help. That round trip (~5 µs) is the same order of magnitude as the
capacity-1 handoff cost measured for the bounded blocking queue's condition variables, it is
the price of one thread parking and another waking it, not something particular to this
class. Against that fixed cost, the pool is still **~3.3x faster** than paying full thread
creation on top of the same round trip, and that gap only widens for shorter tasks.

**Fire-and-forget, many concurrent submitters, pool(4 workers), 100,000 tasks per submitter:**

| submitters | ns/task |
|---|---|
| 1 | 1,580 |
| 2 | 1,212 |
| 4 | 750 |
| 8 | 484 |
| 16 | 481 |

Not waiting on each task individually lets tasks pipeline behind the 4 workers instead of
serializing on a round trip, so throughput improves as more submitters keep the queue
non-empty, up to a point. It flattens hard between 8 and 16 submitters: at four workers,
the queue can only be drained four ways in parallel, and the single mutex protecting `queue_`
is now the shared resource every submitter and every worker contends for. More submitters
past that point buys nothing but more contention on that one lock, the same ceiling the
bounded-queue reading measured for its single mutex.

## 6. Where this solution fails

- **Tasks that submit-and-wait on other tasks from the same pool can deadlock the whole
 pool.** Verified here: `pool(2)`, two outer tasks each submit an inner task to the same
 pool and call `.get()` on it before returning. Both workers are occupied by the outer
 tasks, both blocked waiting on inner tasks neither worker is free to run. Result: **hung,
 still running after 3 seconds**, killed by hand, this is a genuine deadlock, not a slow
 path. The pool has no idea one task is waiting on another; it just sees two occupied
 workers and a queue nobody is pulling from. Never call `.get()` on a pool's own future from
 inside a task running on that same pool unless you can *prove* enough workers are free to
 cover every level of nesting.
- **No priority, no fairness.** `queue_` is strict FIFO and every worker is interchangeable,
 there is no way to say "this task matters more" or to stop one submitter's flood of tasks
 from crowding out another's. A production pool serving multiple callers usually needs
 per-class queues or priorities layered on top.
- **One giant task can starve everything behind it, per worker, not globally.** A worker
 that picks up a task with no natural stopping point runs it to completion before it will
 ever look at the queue again; with N workers, up to N such tasks can each pin one worker
 indefinitely while the rest of the queue backs up behind whichever workers remain.
 Cooperative task design (break long work into resubmitted chunks) or a separate pool for
 long tasks are the fixes, not a smarter scheduler inside this class.
- **`submit()` after the pool is destroyed is not defended against.** Nothing stops a caller
 from holding a reference to a destroyed pool and calling `submit()` on it, that's a
 use-after-free the class does nothing to catch, because ownership of "when is this pool
 gone" is the caller's problem, same as any other object.
- **The queue is unbounded.** A submitter that outproduces the workers indefinitely grows
 `queue_` without limit, this class has no backpressure. That's a deliberate simplification
 (bounding it reintroduces the bounded-queue's own not_full/not_empty design on top of this
 one), but it means a runaway submitter is a memory leak with extra steps, exactly as the
 bounded-queue reading warned about unbounded queues in general.
- **An exception that escapes the lambda `[task]{ (*task)(); }` itself (rather than from
 inside the user's callable) would terminate a worker thread outright**, shrinking the pool
 by one for the rest of its life with nothing to notice or replace it. In practice this
 can't happen here, `packaged_task::operator()` catches whatever the wrapped callable
 throws and stores it in the future instead of letting it propagate, but it is exactly the
 kind of assumption worth stating explicitly rather than leaving implicit, because the whole
 pool's health depends on it staying true.
- **`task()` runs while the queue's mutex is unlocked, which is correct, but only because
 nothing in `worker_loop` re-locks `m_` from inside a task by accident.** A task that
 itself calls back into `submit()` on the same pool (the nested-task deadlock above) is one
 way this bites; a task that somehow reaches back into the pool's private state would be a
 worse one. The class's safety here rests on tasks being self-contained work, not code that
 reaches back into the pool that's running it.

## 7. Interview follow-ups

**"Why join every worker in the destructor instead of detaching them?"** A detached worker
that outlives the pool is a thread still holding a reference to `this`, `m_`, `cv_`,
`queue_`, all about to be destroyed. The very next time it loops back to `cv_.wait(...)` it
reads freed memory. Joining is what makes "the pool is destroyed" and "no thread is touching
the pool's members any more" the same moment.

**"Why `notify_all()` in the destructor and not `notify_one()`?"** Every worker might be
parked in `wait()` at shutdown, and all of them need to re-check the predicate, a single
`notify_one()` might wake one worker, which drains the queue and exits, while the rest never
wake up at all and the destructor's `join()` calls hang forever waiting for threads nobody
told to leave.

**"Small machine, 2 cores, pool sized to `hardware_concurrency()`. What changes?"** The pool
ends up with 2 workers. Correctness is identical, nothing here assumes more than one core,
the mutex and condition variable work the same under one-core time-slicing as under true
parallelism (see the dining-philosophers reading for the general argument). What changes is
that fire-and-forget throughput has far less room to scale, the "more submitters keep
workers pipelined" effect measured in section 5 tops out at 2 workers' worth of concurrent
draining instead of 4, so the flattening in that table happens much earlier.

**"Big machine, 128 cores, pool sized to match. What breaks first?"** The single
`std::mutex` guarding `queue_`. Every `submit()` and every worker's dequeue serialises on it;
section 5's numbers already show the ceiling forming with just 4 workers and 8-16
submitters on 10 cores. At 128 workers all pulling from one queue, that mutex becomes the
whole program's throughput ceiling, the fix is multiple queues (one per worker, or a small
number of shards) with work-stealing between them when one runs dry, which is exactly the
next tier of pool design this course covers separately.

**"A worker's task throws something that escapes `packaged_task::operator()` somehow, say,
a `std::bad_alloc` from deep inside a container the task uses. Walk through what happens."**
It doesn't escape, that's the guarantee `packaged_task` provides. `operator()` wraps the
call in a try/catch and stores any exception, `bad_alloc` included, in the shared state, to
be rethrown by whoever calls `.get()` on that specific task's future. The worker thread
itself never sees the exception and loops around to the next task exactly as if the task had
returned normally. The one thing that *would* take a worker down is an exception thrown by
code in `worker_loop` itself, outside the wrapped call, which is why that lambda is the only
line that needs to be trusted not to throw.

**"Production ops: pool of 16 workers, queue depth is climbing steadily. What do you check,
and what's the fix?"** Watch queue depth and per-task latency as live metrics, a climbing
depth with stable per-task latency means the *arrival rate* exceeds *service rate*
(undersized pool or a workload spike; the fix is more workers, up to core count, or shedding
load), while a climbing depth *with* climbing latency points at one or more tasks not
finishing (the starvation case in section 6, or the nested-submit deadlock, check for
workers that have been running the same task far longer than the workload's normal shape).
Graceful shutdown itself needs a metric too: track "queue depth at the moment shutdown was
requested" so an operator can see how much work a rolling restart is about to wait out before
it forcibly kills the process on a timeout.
