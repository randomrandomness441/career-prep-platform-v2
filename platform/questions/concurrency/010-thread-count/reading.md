## 1. Reframe the problem

Splitting a sum across threads is the easy half. The question this exercise is really
about is the other half: **how many threads?**

You cannot know that when you write the code. It depends on the machine the program ends
up running on and on how much work there actually is. So it becomes a runtime decision
with exactly two inputs:

**Input one, how many threads can genuinely run at the same time.**

```cpp
unsigned n = std::thread::hardware_concurrency();
```

Two things about this function. It is a **hint**, not a promise, and it is explicitly
allowed to return **0**, meaning "the implementation could not work it out". Any code
that does `length / hardware_concurrency()` without checking has a division by zero
waiting for it on some machine you have never seen. On this machine it returns **10**.

Beyond that number, extra threads do not add workers. They add *queue*. The OS starts
time-slicing them onto the same cores: each switch costs a trip through the kernel and
leaves the new thread's data cold in a cache that the previous thread just filled with
something else. That is **oversubscription**, and it is a pure loss.

**Input two, how much work there is.**

Creating a thread on this machine costs roughly 20–35 µs. Summing a thousand integers
costs well under a microsecond. If a thread is handed less work than it costs to create,
you have paid for a worker who finishes before he arrives. So you pick a minimum amount
of work per thread and refuse to go below it:

```
threads = min( hardware_concurrency-or-a-sane-default, ceil(length / min_per_thread) )
```

Small range → the second term wins → one thread → **create nothing at all** and just sum
it on the calling thread. Big range → the first term wins → you use the machine and stop.

One more detail that follows from this: if you decide on `n` threads, you launch `n - 1`
of them. The calling thread is a perfectly good worker, and it has nothing else to do but
wait. Launching `n` and blocking is leaving a core idle.

## 3. The broken version, first

Here is what "make it parallel" usually turns into:

```cpp
std::size_t plan_threads(std::size_t length) { return length; } // a thread per element!

template <typename E, typename T>
T parallel_accumulate(const std::vector<E>& v, T init) {
    const std::size_t length = v.size();
    const std::size_t num_threads = plan_threads(length);
    std::vector<T> results(num_threads);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&v, &results, i] {
            results[i] = std::accumulate(v.begin() + i, v.begin() + i + 1, T());
        });
    }
    for (auto& t : threads) t.join();
    return std::accumulate(results.begin(), results.end(), T()); // init dropped
}
```

Compiled `-O2` and run, this is the actual output:

```
empty range, init=100 -> 0 (std::accumulate: 100)
10 ones, init=100 -> 10 (std::accumulate: 110)
 1000 elements -> sum 1000, 1000 threads, 23.37 ms (serial: 0.00004 ms)
 5000 elements -> sum 5000, 5000 threads, 156.37 ms (serial: 0.00004 ms)
```

Two separate failures, and the second is the interesting one.

**It is wrong.** `init` never gets folded in, so the empty range returns 0 instead of 100.
An empty range is exactly the case the "obvious" code forgets, because there is no loop
iteration to think about.

**It is half a million times slower than the serial loop.** 1000 elements: 23.37 ms
threaded against 0.00004 ms serial. Nothing here is a "parallel overhead of a few percent"
, the work is so much smaller than the machinery around it that the machinery is all you
measure. The 5000-element case took about 6.7x as long as the 1000-element case for 5x the
threads, so creation cost creeps up a little faster than linear too, but the number worth
remembering is not the slope, it's that both figures are enormous next to a serial loop
that doesn't even register on the clock.

### Where the crossover actually is

The same total amount of arithmetic, split `k` ways, on this 10-hardware-thread machine
(each row is the best of 5 runs, `-O2`):

**4,000,000 units of work**, a realistic parallel sum:

| threads | ms | speedup |
|---|---|---|
| 1 | 1.427 | 1.00x |
| 2 | 0.739 | 1.93x |
| 4 | 0.400 | 3.57x |
| 8 | 0.301 | 4.73x |
| **16** | 0.302 | 4.72x |
| 64 | 0.693 | 2.06x |
| 256 | 3.065 | **0.47x** |

Read that bottom row twice. At 256 threads the parallel version is **twice as slow as
doing it on one thread**. The curve does not plateau politely; it goes over a cliff.

Notice also that 8 threads bought 4.73x, not 8x. This machine's 10 hardware threads are
not 10 equal cores, some are efficiency cores that run the same block of work
considerably slower. Equal-sized blocks means everyone waits for the slowest one.

**400,000,000 units of work**, 100x more work, same split:

| threads | ms | speedup |
|---|---|---|
| 1 | 142.2 | 1.00x |
| 2 | 71.0 | 2.00x |
| 4 | 35.4 | 4.01x |
| 8 | 21.1 | 6.75x |
| 16 | 21.3 | 6.67x |
| 64 | 18.3 | 7.77x |
| 256 | 18.7 | 7.62x |

Here 256 threads is *not* a disaster, because each thread still receives 1.5 M units of
work, which dwarfs the ~25 µs it cost to create it. **Oversubscription is not bad in
proportion to the thread count; it is bad in proportion to how small each thread's share
of the work is.** That is the whole justification for the `min_per_thread` threshold.

And when the work is tiny, splitting it costs pure money, same total work of 1000 units,
split `k` ways:

| threads | µs |
|---|---|
| 1 | 19.4 |
| 2 | 31.0 |
| 8 | 82.6 |
| 64 | 683.2 |
| 256 | 3061.1 |
| 1000 | 14119.7 |

Roughly 14 µs of pure overhead per thread, and not one of them had anything worth doing.

The fix is the whole exercise: cap by `hardware_concurrency()` (guarding against 0),
divide by a minimum work quantum, and when the answer is 1 or 0, create nothing.

## 6. Where this solution fails

- **`hardware_concurrency()` is not "cores available to you".** It reports the machine,
 not your share of it. In a container limited to 2 CPUs it still cheerfully reports the
 host's 64. Other processes are invisible to it, and so is the rest of *your own*
 process.
- **Nested parallelism multiplies.** Call `parallel_accumulate` from 8 threads and you
 get 8 x 10 = 80 threads, each of which believes it is being reasonable. Every one of
 them measured the machine and none of them measured each other. The real fix is not a
 better formula, it is a **thread pool**, one place that owns the thread budget for the
 whole process.
- **Static equal-sized blocks strand you behind the slowest core.** Measured above: 8
 threads gave 4.73x, not 8x, largely because the blocks are equal but the cores are not.
 A block that lands on an efficiency core, or a core the OS decides to give to someone
 else halfway through, holds up everyone. The fix is many small chunks and work stealing
 (chapter 9), not a smarter split.
- **`min_per_thread = 1000` is a guess.** It is right only for elements that cost about
 as much to process as an integer add. If each element requires a hash or a file parse,
 1000 is far too high and you leave parallelism on the table; if the loop is memory-bound,
 even 100,000 may be too low. The constant is a stand-in for "measure your workload".
- **The result is memory-bandwidth-bound long before it is core-bound.** Summing a large
 array of `int` saturates memory bandwidth at 3–4 threads; the remaining cores add
 nothing at all. Getting 10 threads onto a 10-core box does not give you 10x, it gives
 you whatever the slowest shared resource allows.
- **Accumulate locally, then store once.** Each worker owning `results[i]` needs no mutex,
 but `results` is one small array, so several `results[i]` share a cache line. Writing to
 it in a loop makes those threads fight over that line. Measured, 20 M elements across
 10 threads: **1.40 ms** accumulating into a local and storing once, versus **2.08 ms**
 writing `results[i] += *it` each iteration, a 1.5x penalty for a line of code that
 looks identical. (Full treatment under false sharing, chapter 8.)
- **An exception in any worker calls `std::terminate`.** `std::accumulate` on a type whose
 `operator+` can throw will kill the process, because an exception escaping a thread's
 entry point is not catchable by the launcher. Carrying it back requires
 `std::packaged_task` or `std::async` (chapter 4).
- **Floating-point sums change value when you split them.** Addition is not associative
 in floating point, so `parallel_accumulate` over `double` gives a *different* answer to
 `std::accumulate`, often a more accurate one, but different, and different again for a
 different thread count. That means the result depends on the machine it ran on.
- **Nothing stops a caller from mutating `v` while a call is in flight.** `v` is taken by
 `const&`, which only stops this function from writing to it, a second thread calling a
 non-const method on the same vector concurrently is a data race regardless.

## 7. Interview follow-ups

**"You measured 256 threads being twice as slow as one thread on the small workload, but
fine on the large one. What's the actual dividing line, in your own words?"** It's not the
thread count that matters, it's the *ratio* of work-per-thread to thread-creation cost,
measured here at ~14µs of pure per-thread overhead when there's nothing worth doing. At
4,000,000 units split 256 ways, each thread gets too little real work to amortize that
overhead; at 400,000,000 units split the same 256 ways, each thread's share (1.5M units)
dwarfs the ~25µs creation cost, so the same thread count that was a disaster at one scale is
free at another. `hardware_concurrency()` bounds the *ceiling*; the `min_per_thread`
threshold is what protects the *floor*.

**"hardware_concurrency() returned 10 on this test machine but the 8-thread row only showed
4.73x speedup, not 8x. Why the gap?"** Two separate reasons, both covered directly: this
machine's 10 hardware threads aren't 10 identical cores (some are efficiency cores running
the same block measurably slower, so equal-sized static blocks strand everyone behind the
slowest one), and the workload itself goes memory-bandwidth-bound well before it's
core-bound, summing a large array saturates memory bandwidth around 3-4 threads, so cores 5
through 10 are fighting over the same bandwidth, not adding independent throughput.

**"Small machine, a container limited to 2 CPUs. Does hardware_concurrency() report that,
and what breaks if you trust it blindly?"** No, `hardware_concurrency()` reports the
*machine's* hardware, not your process's actual allotment; inside a 2-CPU container on a
64-core host, it commonly still returns 64. Trusting it blindly means launching far more
threads than you actually have cores for, all fighting for time on 2 real CPUs, the
oversubscription cost this reading measures directly, except now it happens by surprise
rather than by choice, because the formula's own assumption (thread count ≈ real parallel
capacity) was wrong from the start.

**"You call parallel_accumulate recursively, or from within an already-parallel context, 8
outer threads each spawn their own parallel sum. What happens, and how would you actually
fix it, not just patch the formula?"** Nested parallelism multiplies: 8 outer threads times
however many each inner call decides to launch, and every one of them independently measured
`hardware_concurrency()` and believed it was being reasonable, none of them measured each
other. No smarter formula fixes this from inside `parallel_accumulate` itself, because the
function genuinely has no visibility into what else is running; the real fix is a single
thread pool (see [[032-work-stealing-pool]]) that owns the process's entire thread budget, so
nested calls submit work to the same bounded pool instead of each independently deciding to
create new OS threads.

**"Production ops, this function is deployed and someone reports it's much slower on the
production host than in your benchmarks. What's the first thing you'd check?"** Whether the
production host's actual available core count matches what `hardware_concurrency()` reports
there, cloud instances, containers, and CPU-limited cgroups are exactly the case where this
diverges from a bare-metal benchmark machine, and a formula tuned assuming the reported
number is real parallel capacity will oversubscribe silently in a way that only shows up as
"slower in prod for no obvious reason," not as an error or a crash.
