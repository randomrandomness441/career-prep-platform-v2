# Stopping a Thread That Is Asleep

## ELI5: waking a napping night-shift guard

A night-shift security guard naps between rounds when there's nothing to check, no
point standing around awake for no reason. There are two different reasons someone might
need to wake them: "there's something new to check" (a new job arrived), or "your shift
is over, go home" (stop requested). If the guard's alarm clock only ever listens for the
first kind of alert, and someone tries to send the "go home" signal while the guard is
fast asleep with nothing queued up, the guard just... keeps napping. Forever. You've told
them to stop, and they never heard it, because the only bell wired up to their ears was
the "new job" bell.

That's the exact bug this question is about: a worker that sleeps on a condition
variable while idle, and a stop signal that only gets *noticed* if the worker happens to
already be awake, checking between jobs.

## What you're actually building

`queue_worker`, a background thread that pulls jobs from a queue and blocks on a
condition variable when there's nothing to do:

```cpp
class queue_worker {
public:
    queue_worker(); // starts the worker thread
    ~queue_worker(); // stops the worker and joins, promptly
    void submit(long job); // enqueue a job; jobs are processed FIFO
    void request_stop(); // thread-safe: ask the worker to finish;
    // must WAKE a worker parked on the condition variable
    void join(); // block until the worker has exited; idempotent
    long sum() const; // running sum of processed jobs
    int processed() const; // count of processed jobs
};
```

## Requirements

1. Jobs submitted while the worker runs are processed FIFO; `sum()` and `processed()`
 account for every fully processed job, exactly once.
2. A worker parked on the condition variable with an empty queue must wake **and exit**
 when `request_stop()` is called, from any thread, with nothing else happening. This
 is the sleeping guard, specifically.
3. `join()` must return within a generous bound after a stop request. The destructor
 must stop and join by itself, promptly, even if unprocessed jobs remain queued
 (pending jobs may be processed or dropped, whichever the race decides, but shutdown
 must not hang waiting for a growing backlog, and nothing may be double-processed).

## Why the constraints exist

- **`std::jthread`, `std::stop_token`, and the condition-variable wait that understands
 stop.** A stop flag checked only between jobs is precisely the bug you're replacing,
 that's the guard whose alarm clock only listens for the new-job bell.
- **Know this before the compiler tells you:** the wait overload that takes a stop
 token, `wait(lock, stop_token, pred)`, exists on **`std::condition_variable_any`
 only**. `std::condition_variable` doesn't have it. A guide floating around the internet
 claims otherwise, it doesn't compile.
- **Think about member declaration order.** The thread must be joined *before* the
 mutex, condition variable, and queue it uses are destroyed, the guard has to actually
 go home before you tear down the guardhouse.
