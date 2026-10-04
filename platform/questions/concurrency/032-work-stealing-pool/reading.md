## 1. Reframe the problem

A plain thread pool, one shared queue, every worker locking the same mutex to pop the next
task, has an obvious ceiling: every single pickup, from every worker, funnels through one
lock. Work stealing's insight is that most of the time, a worker doesn't need to touch
anyone else's state at all. It has its own backlog to work through. Give every worker its
own queue, and the common case, a worker grabbing its own next task, becomes lock
contention between at most nobody, most of the time, since nobody else has a reason to
touch that specific queue. Stealing is the fallback, not the common path. Only when a
worker runs dry does it reach into someone else's queue, and even then it only ever touches
one queue's lock at a time.

The second reframe this question needs: a thread pool's shutdown is not "stop soon," it's
"finish every task that was ever promised, then stop." And in C++ specifically, "then stop"
has a sharp edge most people don't think about until it bites them. A `std::thread` object
that's still running when it's destroyed doesn't quietly clean itself up. It crashes the
whole process.

## 2. The tools

### Per-worker deques, opposite ends

Each worker's queue is a `std::deque`, and two different operations touch it from opposite
ends. The owner pushes and pops from the back, LIFO, because a worker tends to keep working
on what it just produced, which is cache-warm. A thief pops from the front, FIFO, the
oldest and most cache-cold work, which the thief was going to have to fetch from memory
anyway. Using opposite ends isn't required for correctness, a single mutex per queue makes
either end safe either way. It's what keeps the owner's own hot path and a thief's
occasional visit from fighting over the same end of the same structure as often.

### `std::thread` and joinability

A `std::thread` object is joinable from the moment it's constructed with a callable, until
either `.join()` (wait for it to finish) or `.detach()` (disown it, letting it run to
completion on its own, untracked) is called on it. If a joinable `std::thread`'s destructor
runs, which happens whenever the object goes out of scope, including as part of a
`std::vector<std::thread>` being destroyed, the standard requires calling
`std::terminate()`. This is intentional, not an oversight: silently blocking (surprising) or
silently abandoning the thread (dangerous) were both rejected in favor of a loud,
unambiguous failure.

## 3. The broken version, first

The boilerplate's worker loop, its steal logic, its queue design, all of it is identical to
the correct version. The entire bug is one destructor:

```cpp
~WorkStealingPool() {
    stop_.store(true, std::memory_order_release);
}
```

**Why it looks right:** it does what "signal shutdown" sounds like it should do: flip a
flag, let the workers notice on their own. For code that's mentally modeling "tell them to
stop" as the goal, this looks complete. The part that's missing, actually waiting for them
to finish, is a separate step that's easy to forget specifically because the flag-flip
feels like the shutdown action.

Running it, 8 workers, 500 trivial tasks, destroyed immediately, inside a forked child so
the crash can't take down the test harness itself:

```
the pool's destructor let a still-running std::thread be destroyed: the process
was killed by signal 6 (Abort trap: 6), consistent with std::terminate()
```

Every run, not an occasional one. `workers_`'s own destructor runs immediately after the
pool's destructor body finishes, and at that point every worker thread is, essentially
always, still actually running. They loop until they see `stop_`, and 500 trivial tasks plus
loop overhead takes real, if brief, time. `std::terminate()` fires reliably, not as a rare
race.

The fix adds exactly the missing step:

```cpp
~WorkStealingPool() {
    stop_.store(true, std::memory_order_release);
    for (auto& t : workers_) t.join();
}
```

Same test: clean exit, every time, plus 4,000 submitted tasks surviving immediate shutdown
with none abandoned, and work confirmed spread across more than one worker thread.

## 4. Real-world usage

**Why work-stealing was invented.** The technique traces to Cilk's scheduler in the 1990s,
built specifically to parallelize recursive, unevenly-sized workloads (like parallel
quicksort's uneven partitions, see [[007-async-future]]) without one thread's local backlog
becoming a bottleneck while others sit idle. A single shared queue can balance load, but
pays a lock on every single pickup to do it. Work-stealing pays that same cost only on the
rebalancing operation itself, a steal, which happens far less often than a plain pop does.

**Where you meet it in production:** the parallel algorithm libraries behind most modern
runtimes, Intel's TBB, Java's `ForkJoinPool`, Rust's Rayon, Go's goroutine scheduler, are all
work-stealing schedulers at their core, precisely because "many small, unevenly-sized,
independent tasks" describes most real parallel workloads: recursive divide-and-conquer,
per-request work in a server, graph traversal.

**Where NOT to reach for it:** a small, fixed number of long-running, roughly equal-sized
tasks (say, 8 threads each doing one big, predictable chunk of work) gets nothing from
work-stealing's rebalancing. There's nothing uneven to rebalance, and a plain `std::thread`
per task, or a plain fixed-assignment pool, is simpler and just as fast. Reach for
work-stealing specifically when task sizes are unpredictable or uneven. It's solving a
load-imbalance problem, not a "run things in parallel" problem in general.

## 5. Performance

400,000 trivial tasks, 8 workers, this machine, three repeats:

```
work-stealing pool (per-worker deque): 158-213 ms
single shared queue (one mutex + one cv): 1198-1205 ms
```

Roughly **6x**, consistently. The shared-queue version pays a lock, and worse, a
condition-variable wait/notify round trip, on every single task handoff, from every worker.
The per-worker-deque version only pays real contention when a worker's own queue is
actually empty and it has to steal, which, for a steady, evenly round-robined stream of
trivial tasks, is rare. This is the direct, measured version of section 1's claim: moving
the common case off a shared lock is where the win comes from, not from anything exotic in
the stealing mechanism itself.

## 6. Where this solution fails

- **Round-robin submission doesn't account for task size.** If task durations are wildly
 uneven, round-robin can still hand a disproportionate amount of work, not just count, to
 one worker's queue before anyone notices there's an imbalance to steal against. The scheme
 only rebalances once a worker actually runs dry, not proactively.
- **A worker with an empty queue scans every other worker's queue in a fixed order every
 time it needs to steal**, an `O(num_threads)` scan on every failed local pop. At large
 thread counts, randomizing the steal order, rather than always starting from `self + 1`,
 avoids every idle worker converging on the same "next" victim at once.
- **A task that throws propagates out of `run()` uncaught here.** Same lesson as this
 question's own destructor bug: an uncaught exception escaping a worker's loop function
 calls `std::terminate()` just as surely as a non-joined thread does. Production code needs
 a `try`/`catch` around each task's invocation, likely reporting the failure back through a
 mechanism like [[048-exceptions-across-threads]]'s rather than letting it propagate.
- **This pool has no bound on queue depth.** `submit` never blocks or refuses. A producer
 that submits faster than the pool can drain grows queues without limit, the same
 backpressure problem [[024-backpressure]] and [[025-thundering-herd]] address for a single
 shared queue. Nothing here revisits that question for the per-worker-queue shape.

## 7. Interview follow-ups

**"Why does a joinable std::thread's destructor call terminate() instead of just joining
automatically?"** An automatic, implicit join in the destructor would silently block the
calling thread for however long the worker takes, potentially forever, with no indication
anything is waiting. The standard committee judged an unambiguous crash safer than a silent,
unbounded hang. `std::jthread` (C++20) is the library's answer to wanting the convenience of
auto-join without writing it by hand. It joins automatically in its destructor, on purpose,
because it's a newer type designed specifically to make that choice explicit and intentional
rather than something you fall into a crash by forgetting.

**"How would you adapt this for std::jthread instead of std::thread?"** `std::jthread`'s
destructor already joins, so the crash this question's boilerplate demonstrates couldn't
happen even with a completely-forgotten join call. But you'd still want an explicit stop
signal, a `std::stop_token`, which `jthread` also supports natively, rather than relying on
destruction order alone, since you generally want workers to finish draining remaining work
before the destructor's implicit join blocks on them, not just eventually stop when they
happen to notice the object is gone.

**"10,000 tiny tasks a second across 8 workers, what's the first thing that breaks in this
design?"** The `O(num_threads)` linear steal scan is cheap at 8 workers. At much higher
thread counts it becomes a real cost paid on every failed local pop. The fix at that scale
is usually a smarter victim-selection strategy, randomized, or a hierarchy of steal domains,
rather than a fixed round-robin scan order. The underlying per-queue-mutex design still
holds, only the "who do I steal from" policy needs to change.

**"A worker's task itself submits new tasks to the same pool, does this design handle
that?"** Yes, by construction: `submit` just needs `this` and doesn't care which thread
calls it, so a task running inside `run()` can call `pool.submit(...)` freely. The new task
lands in some worker's queue, possibly the calling worker's own, exactly like any other
submission. This is actually the common case for recursive divide-and-conquer work, parallel
quicksort, parallel tree traversal, which is precisely the workload shape work-stealing was
invented for.
