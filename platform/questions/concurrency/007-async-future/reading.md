## 1. Reframe the problem

Everything so far has been about *threads*, objects you start, join, and coordinate by
hand. This question is about a different unit: the **task**.

A `std::future<T>` is a **handle to a value that does not exist yet**. You hand work off,
get a receipt, carry on doing something else, and later present the receipt to collect the
answer, blocking only at the moment you actually need it.

The shift matters because it inverts who does the bookkeeping. With threads you manage
lifetime, you arrange for the result to be written somewhere, and you invent a way to move
an exception across the thread boundary. With futures the library does all three. You say
what should be computed, not how it should be scheduled.

## 2. The tools

### `std::async`, run this, give me a receipt

```cpp
#include <future>

std::future<int> f = std::async(std::launch::async, compute, arg);
// ... do other work here ...
int result = f.get(); // blocks only if it is not finished yet
```

`get()` may be called **once**. It moves the result out. Calling it twice is undefined.

### Launch policies, the trap

```cpp
std::async(std::launch::async, f); // guaranteed: runs on a new thread
std::async(std::launch::deferred, f); // guaranteed: does NOT run until .get()
std::async(f); // async | deferred, implementation picks
```

The default is the union of both, which means **the implementation is free to run your
"parallel" code sequentially**, on the calling thread, at the moment you call `get()`.
Nothing warns you. Your speedup is silently 1.0x.

Always spell out `std::launch::async` when you actually want parallelism.

### Exceptions cross the boundary for free

This is the part that makes futures worth using. A raw `std::thread` that throws calls
`std::terminate()`, there is no way to catch it from outside. With a future:

```cpp
auto f = std::async(std::launch::async, []{ throw std::runtime_error("bad"); });
try { f.get(); }
catch (const std::runtime_error& e) { /* caught here, on the calling thread */ }
```

The exception is captured, stored, and rethrown at the point of `get()`.

### `promise` and `packaged_task`, the manual versions

`std::async` is the convenient front end. Underneath:

- **`std::packaged_task<Sig>`** wraps a callable so that invoking it stores the result in an
 associated future. You choose where and when to run it, which is exactly what a thread
 pool needs.
- **`std::promise<T>`** is the raw producer end: you call `set_value()` or `set_exception()`
 by hand. Use it when the result does not come from a function returning normally, a
 network reply, a callback, an event.

```cpp
std::packaged_task<int()> task(compute);
std::future<int> f = task.get_future();
queue.push(std::move(task)); // some pool thread will run it later
int r = f.get();
```

### The template machinery in `spawn_task`, a plain-language glossary

Follow-up part 2 asks you to build a mini `std::async`. Doing that *generically*, for
any callable, any argument list, including move-only things, pulls in some C++ that
looks scarier than the idea behind it. Each piece, in order:

- **`template <typename F, typename... Args>`**, "I don't know yet what type the
 callable is, or how many arguments there are, or what type each one is. Figure it out
 from what's actually passed in." The `...` (a *parameter pack*) is just "zero or more
 of these."
- **`F&& f, Args&&... args`**, a *forwarding reference*. It's not "an rvalue reference";
 in this exact position (`T&&` where `T` is a template parameter being deduced) it means
 "bind to whatever was passed, and remember whether it was a temporary or not," so that
 a move-only argument (like a `std::unique_ptr`) can be moved through instead of copied.
- **`std::forward<F>(f)`**, "pass this along exactly the way it arrived", as a copy if
 it arrived as one, as a move if it arrived as one. Without it, everything would
 silently become a copy, and move-only types wouldn't compile.
- **`std::invoke_result_t<...>`**, "what type does calling `f` with these argument
 types produce?" It's a compile-time question, answered by the compiler, so `spawn_task`
 can write `std::future<R>` with the right `R` without you naming it by hand.
- **`std::decay_t<T>`**, "strip the reference-ness and const off `T`." `Args` might be
 deduced as `int&` or `const std::string&`; you want the *value* type to store, not a
 reference to something that may not outlive the call.
- **`if constexpr (std::is_void_v<R>)`**, an `if` decided at compile time, not runtime.
 It exists here because `std::promise<void>::set_value()` takes no argument, while every
 other `set_value(v)` does, you can't write one call expression that works for both, so
 the compiler picks one branch and throws the other away entirely before compiling it.

None of this is a concurrency concept, it's the price of "works for anything," the same
price `std::async` itself pays internally. The concurrency idea underneath is small:
wrap the call in a `packaged_task`, hand it to a thread, return its future.

### Limiting recursion depth

Recursive `async` needs a brake, or you get one thread per element. The standard trick is a
depth counter: spawn while depth is small, run inline once it is large.

## 3. The broken versions, first

### Broken 1, it is not parallel at all

The boilerplate is a correct quicksort with both halves recursing on the calling thread.
It sorts perfectly. It uses one core out of ten. Measured on 2,000,000 ints:

```
sequential quicksort : 546.5 ms
```

### Broken 2, the default launch policy

```cpp
auto f = std::async(sort_lower, std::move(lower)); // no policy
```

This compiles, reads as parallel, and may run entirely on the calling thread. On some
implementations it launches a thread; on others it defers. The same source gets a 5x
speedup on one machine and 1.0x on another, and nothing in the code changed. Diagnosing
this from a performance report alone is miserable.

### Broken 3, one thread per element

```cpp
auto f = std::async(std::launch::async, parallel_quick_sort, std::move(lower));
```

with no depth limit. A 2,000,000-element vector recurses ~2,000,000 times, and each level
demands a real OS thread. The thread constructor throws `std::system_error` long before
that, and you have converted a sorting routine into a crash.

## 4. Real-world usage

**Why futures were invented.** The idea predates C++ by decades, it comes from the 1970s
and 80s (Baker and Hewitt's "futures", MultiLisp's `future` form) and answers a specific
complaint about raw threads: threads are about *execution*, but programs are usually about
*results*. Writing "run this somewhere, tell me when the answer is ready, and give me the
exception if it failed" using only threads means hand-rolling result storage, a
synchronisation handshake, and exception marshalling for every single task. `future`
packages all three, once, correctly.

**Where you meet them in production:**

- **Thread pools.** `packaged_task` is the standard way a pool returns a value to the
 submitter. Almost every C++ pool implementation has `std::future<T> submit(F&&)`.
- **Fan-out / fan-in.** Issue N independent requests, collect N futures, then `get()` each.
 Total latency becomes the slowest one rather than the sum.
- **Async I/O and RPC.** `std::promise` is how a completion callback delivers a result back
 to code that is waiting for it, the callback calls `set_value`, the waiter's `get()`
 returns.
- **Lazily computed expensive values.** `std::launch::deferred` is genuinely useful: the
 work runs on first `get()` and never runs at all if nobody asks.

**Where NOT to use them:**

- **Not for fine-grained work.** Every `std::async(launch::async)` is a thread creation,
 ~26–36 µs on this machine. Tasks shorter than a millisecond lose. Use a pool.
- **Not for streaming or repeated results.** A future carries exactly one value, once. For
 a sequence of results you want a queue, not a future.
- **Not when you need to cancel.** There is no `future::cancel()`. C++ futures cannot be
 interrupted; you need a `stop_token` or a flag the task checks.
- **Not `shared_future` by default.** Reach for it only when several threads genuinely must
 read the same result; it costs an atomic refcount.

## 5. Performance

2,000,000 random ints, this machine (10 hardware threads):

| | time | vs sequential |
|---|---|---|
| sequential quicksort | 546.5 ms | 1.00x |
| parallel quicksort (`std::async`) | 110.9 ms | **4.93x** |
| `std::sort`, single-threaded | **57.1 ms** | 9.6x |

The 4.93x is a real speedup and it is roughly what you should expect: never the full 10x,
because partitioning is serial, the halves are uneven, and every level allocates.

**Now look at the third row.** The single-threaded standard library sort is nearly **twice
as fast as the ten-core parallel version**. Ten cores lost to one.

The reason is the algorithm, not the threading. This quicksort allocates two fresh vectors
at every level of recursion and copies elements into them; `std::sort` is an in-place
introsort that touches memory once and stays in cache. Parallelism multiplied a bad
constant factor by 5. `std::sort` removed the bad constant factor entirely.

That is the most useful lesson in this question: **a 5x parallel speedup can still be a 2x
loss.** Always measure against the best serial implementation, not against your own serial
implementation. Optimising the algorithm usually beats parallelising it, and it is the
cheaper of the two.

## 6. Where this solution fails

- **`std::async`'s future has a blocking destructor.** This is unique to `async` and
 surprises everyone. If you do not store the returned future, the temporary is destroyed at
 the end of the statement, and its destructor *waits for the task to complete*, so
 `std::async(launch::async, f); std::async(launch::async, g);` runs f and g strictly one
 after the other. Futures from `promise` and `packaged_task` do not behave this way.
- **The default launch policy.** Covered above; assume nothing without naming the policy.
- **No cancellation.** Once launched, it runs to completion. A task that blocks forever
 blocks your `get()` forever, and the destructor blocks too.
- **Thread explosion without a depth limit.** The recursion must stop spawning at some
 depth, and the right depth depends on the machine.
- **`get()` is one-shot.** Second call is undefined behaviour. Use `shared_future` when
 several readers need the value.
- **Unbalanced partitions.** Quicksort on already-sorted input with a first-element pivot
 degenerates to O(n²), and the parallel version degenerates with it, the "parallel" half
 is empty every time, so you spawn a thread to sort nothing while one thread does all the
 work. Median-of-three pivoting matters more here than threading does.

## 7. Interview follow-ups

**"You measured the parallel version losing to single-threaded std::sort by 2x. If a
teammate showed you a 5x parallel speedup number in a PR, what would you ask before
approving it?"** What it was compared against. A 5x speedup over your *own* unoptimized
serial version proves nothing about whether the result beats the best available serial
implementation, this reading's own numbers are the concrete counterexample: 4.93x
speedup, and still a 2x loss to `std::sort`. Always ask "5x faster than what," and specifically
whether anyone tried the standard library's own implementation first.

**"Why does std::async's future have a blocking destructor, when promise/packaged_task's
futures don't?"** It's specific to the *guarantee* `std::launch::async` makes: the task is
running on its own thread right now, and if nobody's going to `join()`-equivalent it, the
language needs some way to guarantee the thread is cleaned up before the program can
observe the future as gone. `promise`/`packaged_task` make no such lifetime promise about
where or when the associated work runs, that's the caller's own responsibility (see
[[032-work-stealing-pool]] for what happens when a thread's own lifetime isn't managed
carefully), so there's nothing for their futures' destructors to block on.

**"Small machine, 1-2 cores. Does spawning std::async tasks recursively still make
sense?"** The depth-limiting logic (spawn while `depth > 0`, run inline once you're out)
still needs to exist, but the depth itself should scale down with `hardware_concurrency()`
, on 1-2 cores, spawning even a handful of parallel tasks buys nothing (there's no second
core for them to actually run on simultaneously) and only adds thread-creation overhead on
top of the same serial work. The code in this reading already computes its depth from
`hardware_concurrency()` for exactly this reason, don't hardcode a depth tuned for a
10-core machine.

**"You're building spawn_task on packaged_task instead of std::async, why does that avoid
the recursive-thread-explosion risk async has?"** It doesn't automatically, `spawn_task` as
built here launches a raw `std::thread` per call with no depth limiting at all, which means
naive recursive use would exhaust the OS's thread limit exactly like unbounded `std::async`
recursion does. The advantage `packaged_task` brings isn't automatic throttling, it's that
you *choose* where the task runs (a thread, a pool, deferred), which means you can plug it
into a bounded work-stealing pool (see [[032-work-stealing-pool]]) instead of a raw thread,
getting the depth-limiting for free from the pool's fixed worker count rather than having to
hand-roll it the way this question's quicksort part does.

**"Production ops, a service using std::async(launch::async) recursively started throwing
std::system_error under load it hadn't seen in testing. What happened, and how would you
have caught it earlier?"** Almost certainly thread exhaustion, recursive `async` with no
depth limit spawns roughly one thread per leaf of the recursion, and under production data
volumes (larger inputs than test fixtures typically use) that count can blow past the OS's
thread limit long before it does in a smaller test run. Catching it earlier means testing
with production-scale input sizes specifically, not just correctness-sized ones, or
statically capping recursion depth so the failure mode becomes "runs somewhat sequentially
past a depth limit" instead of "the constructor throws."
