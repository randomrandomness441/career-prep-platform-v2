## 1. Reframe the problem

You cannot stop a thread. Not from outside, not safely, a thread killed mid-stride
leaves mutexes locked, heaps corrupted, invariants half-updated. What you can do is
*ask* it to stop, at a point of its own choosing. That is why the C++20 machinery is
called **cooperative** cancellation, and it reframes the whole problem:

- the *stopper* needs a channel that means "please finish", `std::stop_token`;
- the *worker* needs to check that channel at points it knows are safe;
- and the hard part, the one this exercise is about: **what if the worker is asleep?**

A thread busy in a loop can check a flag. A thread parked inside a condition-variable
wait checks nothing, it is not running. So the real question is not "how do I set a
flag", it is "how do I make *the act of stopping* wake the sleeper". Everything else in
this question is plumbing, including the other thing `std::jthread` fixes for free: a
thread handle that joins in its destructor instead of crashing the process when you
forget.

## 3. The broken version, first

The naive version is not lazy, it sets a flag, it locks properly, it even calls
`notify_all()` before joining. Someone thought about this:

```cpp
void run() {
    std::unique_lock<std::mutex> lk(m_);
    for (;;) {
        while (!jobs_.empty()) { /* process a batch */ }
        if (stop_) return; // checked between batches
        cv_.wait(lk, [this] { return !jobs_.empty(); }); // and this is where it lives
    }
}
~queue_worker() {
    { std::lock_guard<std::mutex> lk(m_); stop_ = true; }
    cv_.notify_all(); // "I even woke it"
    th_.join(); // ...which never returns
}
```

**Why it seems correct:** the author anticipated the sleeping problem, there's the
`notify_all`, right there. And in the easy case it genuinely works: if the queue still
has jobs, the worker finishes the batch, hits `if (stop_)`, returns, joins, done. Run
the tests with work still in flight and everything passes. The failure needs the one
state nobody tests: **queue empty, worker parked**.

Here is that case in the harness (the run is killed by the watchdog at 12 seconds):

```
TIMED OUT, the program stopped making progress. Deadlock, or waiting on something
that never arrives.
```

And a sample of the still-running process, three seconds in, tells you exactly where
both threads are:

```
main thread:
 main (in tests)
 std::thread::join() (in libc++.1.dylib)
 _pthread_join (in libsystem_pthread.dylib)
 __ulock_wait (in libsystem_kernel.dylib)

worker thread:
 queue_worker::run() (in tests)
 std::condition_variable::wait(...) (in libc++.1.dylib)
 __psynch_cvwait (in libsystem_kernel.dylib)
```

Both asleep, forever. The notify really did fire, and the worker woke up, because
that is what a notification does. But a condition-variable wait does not *leave* on a
notification. It leaves when its **predicate** is true. The predicate asks "is the
queue non-empty?", the answer is still no, so the worker rolls over and the joiner
waits on a thread that will never move again. The flag and the predicate are two
channels, and the worker only listens to one of them.

**The fix** is C++20's `std::jthread` + `std::stop_token` + one specific wait overload:

```cpp
void run(std::stop_token st) {
    std::unique_lock<std::mutex> lk(m_);
    while (cv_.wait(lk, st, [this] { return !jobs_.empty(); })) {
        while (!jobs_.empty()) { /* drain */ }
    }
} // wait returned false: stop requested, queue empty -> exit
```

Three things changed, each load-bearing:

- `std::jthread` passes a `stop_token` to any thread function that accepts one as its
 first parameter. `request_stop()`, callable from any thread, including the
 destructor, flips it. The destructor then joins. You write no `~queue_worker` at
 all.
- The wait is on `std::condition_variable_any`, not `std::condition_variable`, because
 **the overload `wait(lock, stop_token, pred)` exists only on `condition_variable_any`**.
 A certain study guide claims otherwise; here is the compiler on this machine when you
 try it with the plain one:

 ```
 error: no matching member function for call to 'wait'
 cv.wait(lk, st, [] { return false; });
 ~~~^~~~
 note: candidate function template not viable: requires 2 arguments, but 3 were provided
 ```

- The stop becomes part of what the waiter waits on. Internally this wait registers a
 stop callback whose whole job is to notify; the moment `request_stop()` fires, the
 parked thread wakes, sees the token, finds the predicate still false, and, this is
 the difference, *returns false* instead of going back to sleep. Smallest possible
 demonstration, run on this machine (the predicate is never true):

 ```
 main: requesting stop
 worker woke: pred=0 stop_requested=1
 ```

Two footnotes that matter. First, member order: the `jthread` must be declared **last**
so it is destroyed **first**, its stop+join then runs while the mutex, cv and queue
still exist; the reverse order joins a thread through destroyed machinery. Second, the
loop shape above drains the queue *before* re-waiting, so on stop it finishes whatever
batch it caught and exits when it next finds the queue empty, shutdown never waits on
a backlog that keeps growing.

## 6. Where this solution fails

- **Cooperation means the worker chooses when.** A thread grinding through a
 ten-second computation without checking the token stops in ten seconds. Stop latency
 is exactly the distance between your checks, so put them where the work is
 resumable, not where they are cheap.

- **A blocking syscall does not hear the token.** `read()`, `recv()`,
 `sleep_for(10s)`: `request_stop()` cannot interrupt them, because the token only
 reaches waits it was handed. The production fix is to *not block outside token-aware
 waits*: replace a sleep with a timed `condition_variable_any` wait on the token, and
 give blocking I/O its own timeout or select loop.

- **The destructor can still hang forever.** `jthread` joins; join has no timeout. A
 worker that ignores the token turns RAII back into a deadlock with better manners.
 If shutdown must be bounded, there is no standard timed join, you request stop,
 poll `joinable()` against a deadline, and then either detach (accepting that the
 thread outlives the data it touches) or tear down the process.

- **Your own stop callbacks run on the stopper's thread.** `request_stop()` executes
 registered callbacks inline. The cv's internal one is a cheap notify, but a
 heavyweight user callback makes every `request_stop()` caller wait for it.

- **An exception escaping the thread function still terminates the process.**
 `jthread` joins on destruction; it does not catch. Wrap the body of `run` or move
 the exception across with `std::promise`/`packaged_task`.

- **`detach()` remains available and remains almost always wrong.** A detached thread
 that checks a token nobody holds a handle to anymore is a leak wearing a seatbelt.

## 7. Interview follow-ups

**Q: Why is `std::jthread` safer than `std::thread`?**
A: Two things: it joins in the destructor (a plain `std::thread` destroyed while
joinable calls `std::terminate`), and it owns a stop state, so the thread function can
take a `std::stop_token` and be cancellable by convention rather than by hand-rolled
flag.

**Q: Why is cooperative cancellation better than `pthread_kill` / `pthread_cancel`?**
A: Because only the worker knows where it is safe to stop. External killing can land
between a malloc and its bookkeeping, or after a lock and before the data it protects
is consistent, corruption, not cancellation. Cooperative stopping runs at points the
worker chose, where invariants hold. It costs latency; that is the price of not
corrupting the heap.

**Q: The worker is blocked in `cv.wait()`. How does cancellation reach it?**
A: The token-aware wait, `condition_variable_any::wait(lock, stop_token, pred)`. It
registers a stop callback that notifies, so `request_stop()` wakes the sleeper; the
wait then returns the predicate's value, false meaning "stop with nothing to do". On a
plain `std::condition_variable` you would hand-roll: notify on stop and add
`!st.stop_requested()` to the predicate, which is racy to get right and exactly why
the overload exists.

**Q: One core, mostly busy, does the stop get slower?**
A: Marginally. Waking a parked waiter is one syscall (`__ulock_wait` on this machine) and the
scheduler does the rest; on a saturated single core the stopper and the worker compete
for the CPU, so latency becomes scheduling latency. The *bug* in the naive version is
structural, not timing, more cores don't fix a predicate that ignores the stop, and
one core doesn't cause it.

**Q: 64 cores, a pool of 64 workers all parked on one cv, and you stop them all.**
A: Every waiter shares one stop state per `jthread`; each `request_stop()` fires its
own callback. If they share one `condition_variable_any`, one stop's notify_all is a
thundering herd, 63 threads wake, relock the mutex one at a time, and re-park on
*their* tokens being unset. Fine at shutdown once; a disaster if "stop and restart" is
a hot path. Per-worker condition variables scale; one shared one does not.

**Q: Process exit with 1000 jthreads. What is shutdown time?**
A: The sum of 1000 request_stop + join pairs, executed in reverse destruction order,
and one worker that ignores its token hangs the whole exit, because the next join
never starts. This is why production servers export *shutdown duration* as a metric
and run a shutdown watchdog that dumps stacks and exits hard past a deadline.

**Q: SIGTERM arrives. Can the handler call `request_stop()`?**
A: Not directly, the stop machinery allocates and takes locks; signal handlers may
only use async-signal-safe operations. The standard shape: the handler writes a byte
to a self-pipe or sets a `sig_atomic_t` flag, and a dedicated thread watching it calls
`request_stop()` from normal context.

**Q: In a thread pool, threads persist but tasks come and go. Whose token does a task
check?**
A: The task's own, not the thread's. The pool hands each task a token derived from its
own stop source (`std::stop_source`), so cancelling one task cancels one task. A task
that consults the *worker thread's* token couples unrelated work, and remember the
`thread_local` trap: state that survives between tasks because the thread does. Those
two mistakes combined are how one cancelled request takes down a queue it never
touched.

**Q: Where did this API come from?**
A: `stop_token` grew out of the cooperative-cancellation work (P0660) intended for the
executors/thread-pool proposal, cancellation was designed pool-first, the pool
missed C++20, and `jthread` kept the machinery. That history is why the token
integrates with `condition_variable_any` and composes into task-level cancellation,
not just thread-level.
