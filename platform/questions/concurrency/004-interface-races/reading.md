## 1. Reframe the problem

You will hear "make the class thread-safe" and reach for a mutex in every method. That
instinct produces a class where every *method* is safe and the *class* is useless, because
callers cannot do anything with one method. They have to ask a question and then act on the
answer, and between the question and the action the lock is not held.

So the real statement of the problem is: **an operation is only atomic if the caller can
perform it in one call.** Everything you leave the caller to compose out of two calls is a
race you handed them. Fixing this is not a locking problem, you cannot lock your way out
of it, and the mutex is already in the right place. It is an *interface design* problem.
The fix is to change what the class offers, and to delete the methods that tempt people
into two-step usage.

Williams calls these *race conditions inherent in the interface*. They are the hardest bugs
in this chapter, because every tool you own says the code is fine.

## 2. The tools, from scratch

### `std::mutex` and `std::lock_guard`

A mutex is a token that exactly one thread can hold at a time. `lock()` takes it, blocking
until it is free; `unlock()` puts it back.

```cpp
std::mutex m;
m.lock();
// ... only one thread is here at a time ...
m.unlock();
```

You never write those two calls by hand, because any `return`, `break`, or thrown exception
in between skips the `unlock()` and freezes the program forever. `std::lock_guard` locks in
its constructor and unlocks in its destructor, so the unlock happens on every exit path:

```cpp
{
    std::lock_guard<std::mutex> g(m); // locked here
    do_the_thing(); // even if this throws...
} // ...unlocked here
```

The scope of the guard is the critical section. That is the whole point and it is also
where the bug in this question lives: **a `lock_guard` inside a method releases the mutex
when the method returns.** Three locked methods called in a row is three separate critical
sections with two gaps between them, not one big one.

`mutable std::mutex m_;`, declare it `mutable` so `const` methods like `empty()` can lock
it. Locking is not a logical modification of the object.

### `std::optional<T>`, a value or nothing, in one return

```cpp
#include <optional>

std::optional<int> maybe(bool yes) {
    if (!yes) return std::nullopt;
    return 42;
}

std::optional<int> v = maybe(true);
if (v) std::printf("%d\n", *v); // operator bool, then operator*
if (v.has_value()) { int x = v.value(); (void)x; }
```

`optional<T>` is `T` plus a flag, stored inline, no allocation. It is how you make one
return value answer both "was there anything?" and "what was it?", which is precisely what
lets you collapse two calls into one.

### `std::shared_ptr<T>` and `std::make_shared`

A reference-counted pointer. Copying one bumps a counter; the object dies when the last
copy does.

```cpp
#include <memory>
std::shared_ptr<int> p = std::make_shared<int>(7); // allocates once, holds int + count
std::shared_ptr<int> q = p; // count is now 2, no int copied
```

The property that matters here: **moving or copying a `shared_ptr` never touches the
object it points at, and never throws.** `make_shared` can throw, it allocates and it runs
`T`'s constructor, but that happens where you call it, not later. This is the lever for
part 1.

### Moving out of a container

```cpp
std::vector<T> d;
T value = std::move(d.back()); // steals the element's guts; d still has a (hollow) element
d.pop_back(); // now it is really gone
```

Two separate steps, and the order of the *other* things you do around them decides whether
a thrown exception is survivable.

### Enough to solve part 0

One mutex, one `lock_guard`, one `optional`:

```cpp
std::optional<T> pop() {
    std::lock_guard<std::mutex> g(m_);
    if (data_.empty()) return std::nullopt;
    std::optional<T> result(std::move(data_.back()));
    data_.pop_back();
    return result;
}
```

One lock, held across the check and the removal. And crucially: no `top()` exists, so
nobody can write the two-step version.

## 3. The broken version, first

Here is the naive stack. Read it looking for a bug and you will not find one, every method
locks.

```cpp
class threadsafe_stack {
    mutable std::mutex m_;
    std::vector<int> data_;
public:
    void push(int v) { std::lock_guard<std::mutex> g(m_); data_.push_back(v); }
    bool empty() const { std::lock_guard<std::mutex> g(m_); return data_.empty(); }
    int top() const { std::lock_guard<std::mutex> g(m_);
        if (data_.empty()) return -1;
        return data_.back(); }
    void drop() { std::lock_guard<std::mutex> g(m_); if (!data_.empty()) data_.pop_back(); }
};
```

The bug appears only when you try to *use* it. There is exactly one way to take an item off
this stack:

```cpp
while (!s.empty()) { // 1. is there anything?
    int v = s.top(); // 2. what is it?
    s.drop(); // 3. take it
    got[t].push_back(v);
}
```

2000 values, four threads draining. Compiled `-O1`, four consecutive runs:

```
--- run 1
pushed 2000, popped 2002
top() read an empty stack: 0 times
handed to two threads: 11 values (first: 76)
removed but never returned: 10 values (first: 256)
--- run 2
pushed 2000, popped 2002
top() read an empty stack: 1 times
handed to two threads: 7 values (first: 279)
removed but never returned: 6 values (first: 379)
--- run 3
pushed 2000, popped 2001
top() read an empty stack: 0 times
handed to two threads: 7 values (first: 28)
removed but never returned: 8 values (first: 1)
--- run 4
pushed 2000, popped 2002
top() read an empty stack: 1 times
handed to two threads: 11 values (first: 21)
removed but never returned: 10 values (first: 20)
```

Every run corrupts. Two threads see the same top element and both process it. One of them
then removes the *next* element, which nobody ever looked at. In a work queue that means a
job runs twice and a different job silently never runs. `top() read an empty stack` is the
third variant: the stack was drained entirely between the `empty()` that said "yes" and the
`top()` that went to look. Without the `if (data_.empty())` guard that line is
`data_.back()` on an empty vector, which is undefined behaviour, not a wrong number.

### Now the part that should bother you

Rebuild the exact same program with ThreadSanitizer and run it five times:

```
$ clang++ -std=c++20 -g -O1 -fsanitize=thread naive_stack.cpp -o naive_tsan
$ for i in 1 2 3 4 5; do ./naive_tsan; done
pushed 2000, popped 2002
top() read an empty stack: 1 times
handed to two threads: 231 values (first: 0)
removed but never returned: 259 values (first: 10)
... (four more runs, same shape) ...
```

**Not one ThreadSanitizer warning.** Hundreds of values duplicated and hundreds lost, and
the race detector is completely silent.

That is not a TSan defect. TSan looks for two threads touching the same memory without
synchronisation between them. Here there *is* synchronisation on every single access, the
mutex, so by TSan's definition there is no data race. What went wrong is at a level TSan
cannot see: the program's own invariant, "each pushed value is returned exactly once", is
broken by a legal sequence of correctly-synchronised operations.

Note also that TSan made it *much worse* (230 corruptions instead of 10), because its
instrumentation stretches the gap between the calls. That is a useful reflex to have: a
tool that slows threads down changes which schedules you see.

### The fix

Stop letting the caller compose. Give them the whole operation:

```cpp
std::optional<T> pop() {
    std::lock_guard<std::mutex> g(m_); // one lock
    if (data_.empty()) return std::nullopt; // check
    std::optional<T> result(std::move(data_.back()));
    data_.pop_back(); // and removal, with no gap
    return result;
}
```

and **delete `top()`**. Deleting it is the part people skip and it is not optional. A value
you can read without removing is a value another thread may already have taken; every
correct use of `top()` on a shared stack is a use where the answer might already be wrong
by the time you have it.

`empty()` may stay, but only for logging and metrics. The moment a branch depends on it,
the bug is back.

### The second broken version: losing the element

Fix the interface and there is a second, quieter failure. Return by value and `pop()` has
to do this:

```cpp
T value = std::move(data_.back()); // 1. element is now in a local
data_.pop_back(); // 2. the stack no longer has it
return value; // 3. copy/move to the caller, CAN THROW
```

If step 3 throws, the local is destroyed during unwinding. The stack does not have the
element. The caller does not have it. Nobody has it. Compare that with a failed `push()`,
which is harmless because the caller still owns their value.

Running part 1's harness against exactly this implementation, with a `T` whose second copy
throws:

```
pop() with copy budget 1: id 5 appeared 0 times, expected once
 the pop threw "T's copy constructor threw"
 the element was removed from the stack and then lost while being handed to the caller
 nothing in the program still holds it, so it cannot be retried or recovered
```

`bad_alloc` is the realistic trigger, any `T` that owns a heap buffer (`std::string`,
`std::vector`, a `Message` with a payload) allocates in its copy constructor. Under memory
pressure this fires in production and the symptom is "we lost a job", weeks later.

Williams' fix is to arrange for `pop()` to perform **no `T` operation at all**. Store
`shared_ptr<T>` in the container, built during `push()`:

```cpp
std::shared_ptr<T> pop() {
    std::lock_guard<std::mutex> g(m_);
    if (data_.empty()) return nullptr;
    std::shared_ptr<T> result = std::move(data_.back()); // pointer move
    data_.pop_back(); // destroys a null pointer
    return result; // pointer move
}
```

Every line is `noexcept`. There is no window because there is nothing that can throw in it.

The reference overload gets there from the other side: the caller already owns the storage,
so do the risky thing *first* and only remove the element once it has succeeded.

```cpp
bool pop(T& out) {
    std::lock_guard<std::mutex> g(m_);
    if (data_.empty()) return false;
    out = *data_.back(); // if this throws, the next line never runs
    data_.pop_back();
    return true;
}
```

This is why the book's stack looks strange the first time you see it, two overloads, one
returning a pointer, neither returning a `T`. It is not clumsiness. Returning `T` by value
is the one thing that cannot be made safe.

## 4. Real-world usage

**Why the pattern exists.** Barbara Liskov and Jeannette Wing's work on behavioural
subtyping in the late 1980s formalised what a class promises about its own consistency, and
the idea that a "safe" component is one whose *observable* operations each take it from one
valid state to another. The concurrent version of that idea reached mainstream practice with
Java's `java.util.concurrent` (Doug Lea, 2004), where `ConcurrentMap` shipped `putIfAbsent`,
`replace` and `remove(key, value)`, methods that exist for no reason other than that
`containsKey` followed by `put` is a race. C++ arrived at the same conclusion later:
`std::map::try_emplace` and `insert_or_assign` (C++17) are the single-threaded echo of it,
and `std::atomic::compare_exchange` is the hardware-level echo.

The general rule those APIs are all instances of: **the atomic unit of the interface must
be the atomic unit of the caller's logic.**

**Where you meet this in production:**

- **Job queues and thread pools.** `try_pop()` returning `optional<Job>` is universal.
 Nothing exposes `queue.front()`, the entire point of the pool is that a job is claimed
 by exactly one worker, and `front()` claims nothing.
- **Connection and buffer pools.** `acquire()` hands you a connection *and* marks it taken.
 There is no `is_available()` you can branch on.
- **Filesystem and OS APIs.** `open(O_CREAT|O_EXCL)` exists because "stat then create" is a
 race, and it is a security hole with a name (TOCTOU, time of check to time of use).
 `mkstemp` exists for the same reason. This is not a C++ problem; it is an interface
 problem that shows up wherever there is a shared resource.
- **Databases.** `INSERT ... ON CONFLICT DO UPDATE` is the same fix at the SQL layer.
 `SELECT` then `INSERT` in two statements is the same bug at a larger scale.
- **`shared_ptr` returns specifically.** `std::atomic<std::shared_ptr<T>>` (C++20) and every
 lock-free container that hands out ownership use exactly this trick, hand back a pointer
 to an object that already exists, so the handoff cannot fail.

**Where NOT to use this:**

- **Single-threaded containers.** `std::stack` is right to have `top()` and `pop()`
 separately. It costs nothing there, it lets you inspect without paying for a copy, and it
 is what everyone expects. Do not "harden" code that is not shared.
- **When the caller genuinely needs a multi-step transaction.** "Pop two items and push
 their sum" cannot be expressed as a fixed set of atomic methods. At that point the class
 should expose the lock (or a `with_lock(fn)` method that runs a callback under it), or you
 should be using a transaction object. Adding one bespoke atomic method per caller is a
 losing race.
- **When you need blocking rather than a `nullopt`.** `pop()` returning "nothing there" is
 right for a stack being drained. For a producer/consumer queue the consumer wants to
 *wait*, not spin on `nullopt`. That needs a condition variable, a different question.
- **`shared_ptr` returns when `T` is trivially copyable.** Measured below: the allocation
 costs about 3x the whole rest of the operation. For `int`, `optional<int>` is the right
 answer and the exception-safety argument is vacuous because `int`'s copy cannot throw.

## 5. Performance guarantees

Measured on this machine (Apple clang 21, arm64, 10 cores), `-O2`, 2,000,000 pushes and
2,000,000 pops, single-threaded, best of three runs:

| Implementation | ns per operation |
|---|---|
| one lock, `optional<int> pop()` | **7.9** |
| three locks (`empty()` + `top()` + `drop()`) | **14.9** |
| one lock, `shared_ptr<int> pop()` | **25.7** |

Three things fall out of this.

**The correct version is the fast one.** Collapsing three locked calls into one is not a
safety tax, it is a 1.9x speedup, you were paying for three lock/unlock pairs per item and
now you pay for one. This is the unusual case where the safe design and the fast design are
the same design, and it is worth knowing because the reflex "safety costs performance" is
what stops people from doing it.

**An uncontended `lock`/`unlock` pair is cheap.** Roughly 8 ns covers a lock, a `vector`
`push_back` or `pop_back`, and an unlock; the difference between one lock and three is about
7 ns, so a lock/unlock round trip is ~3.5 ns, about 12 cycles. Uncontended mutexes are not
expensive. Contended ones are, see below.

**`shared_ptr` costs about 3x.** 25.7 ns vs 7.9 ns, and essentially all of the difference is
the `make_shared` allocation in `push()`. That is the price of part 1's exception guarantee.
For `int` it buys nothing; for a `T` that allocates in its own copy constructor, the object
was going to be heap-allocated anyway and the relative cost collapses.

### Contention

Draining 2,000,000 items with N threads through one mutex, best of three:

| Threads | ns per pop | M pops/s |
|---|---|---|
| 1 | 8.5 | 118 |
| 2 | 15.9 | 63 |
| 4 | 25.5 | 39 |
| 8 | 18.8 | 53 |
| 10 | 18.7 | 54 |

**More threads never made it faster.** Not once. The single-threaded case is the peak, and
adding a second thread halves throughput. That is what a single global lock does: the
critical section is serialised by construction, so the only thing extra threads contribute
is handoff cost, cache lines bouncing between cores, and the mutex's futex-style
park/unpark when the spin phase gives up.

The partial recovery from 4 to 8–10 threads is not scaling. It is threads *parking*: past a
certain contention level libc++'s mutex stops spinning and puts waiters to sleep, so fewer
cores are actively fighting for the same cache line and the ones that do get through move
faster. Throughput at 10 threads is still 2.2x worse than at 1.

The lesson to take away: fixing the interface race makes the class *correct*, and correct is
the prerequisite. It does not make it *scale*. A single-mutex container has a hard ceiling
at one thread's worth of critical section. Getting past that needs per-thread structures,
sharding, or a lock-free design, later chapters.

## 6. Where this solution fails

- **`empty()` is a lie the moment it returns, and it is still in the class.** Any caller who
 writes `if (!s.empty()) { auto v = *s.pop(); }` has reinvented the bug in a form that
 happens to be survivable (`*` on a `nullopt` optional is UB, so actually it has not
 survived). Keeping `empty()` is a judgement call: it is genuinely useful for metrics and
 genuinely dangerous for control flow. Some codebases delete it entirely and expose only
 `size_hint()` with a name that admits what it is.
- **It does not compose.** "Pop, and if the value is even push it back" is two atomic
 operations, and another thread can run in between. You have made each *method* atomic and
 left every *combination* racy. There is no way to fix this from inside the class. If your
 callers need transactions, they need access to the lock.
- **A `nullopt` return cannot be waited on.** A consumer that wants to block until work
 arrives has to poll, which burns a core. This class is a stack, not a queue; turning it
 into a work queue requires a condition variable and a shutdown protocol, and the shutdown
 protocol is where most real implementations get it wrong.
- **One mutex means no scaling.** Measured above: 2.2x worse at 10 threads than at 1. If the
 stack is the hot path of a 10-core program, this design is correct and too slow, and no
 amount of tuning fixes it, the structure is wrong for the workload.
- **`push()` under `-fno-exceptions`, or with a `T` whose destructor throws.** The
 exception-safety argument assumes exceptions work. In a `-fno-exceptions` build the
 `shared_ptr` version is pure overhead.
- **The copy in `pop(T& out)` is a real cost.** Strong exception safety there is bought with
 a full copy of `T` instead of a move. For a `T` holding a megabyte that is a bad trade and
 you should use the `shared_ptr` overload instead.
- **`std::vector` as the backing store re-allocates.** A `push()` that grows the vector
 moves every element while holding the lock, so one unlucky `push()` in a million takes
 O(n) with every other thread blocked. `std::deque` bounds that; a pre-`reserve`d vector
 avoids it if you know the maximum.
- **Copy construction and assignment are deleted, which is a real limitation.** Copying a
 thread-safe container is genuinely hard, you would have to lock the source, and locking
 two of them (`a = b` while another thread does `b = a`) is the deadlock in question 005.
 Deleting them is the honest answer, but it means this stack cannot be a member of a
 copyable type.

## 7. Interview follow-ups

**"TSan found nothing on the broken version, even with hundreds of corrupted values. Why,
precisely, isn't finding races exactly TSan's job?"** TSan's definition of a race is two
threads touching the same memory with no happens-before relationship between them. Every
access here goes through the mutex, so every individual access *is* properly synchronized,
TSan has nothing to flag. The bug lives one level up, in the *program's own invariant*
("each pushed value is returned exactly once"), which no per-access race detector can check,
because it's not a property of any single memory access, it's a property of a sequence of
them across two separate, individually-correct critical sections. This is the single most
important idea in this question: TSan proves the absence of data races, not the absence of
bugs.

**"You measured that TSan made the corruption WORSE, not just visible, why would
instrumentation change the outcome?"** TSan's bookkeeping around every synchronized access
adds real time, which widens the window between `empty()`'s critical section ending and
`top()`'s beginning, exactly the gap this bug lives in. A tool that slows a program down can
change which schedules actually occur, for better or worse; here it made the already-broken
window even easier to hit. The general lesson: never assume a bug that "doesn't reproduce
under the debugger/sanitizer" is actually rare, the instrumentation itself is part of the
schedule.

**"Small machine, 1-2 cores. Does the interface race still happen?"** Yes, unchanged, the
race is about *scheduling* between the `empty()`/`top()`/`drop()` calls, not about genuine
hardware parallelism. Even one core, executing threads in whatever slices the OS gives them,
can preempt one thread between `empty()` returning and `top()` running, letting a second
thread's full three-call sequence run to completion first. Fewer cores changes the odds, not
the possibility.

**"10 cores, and you've collapsed the interface to one lock, you measured throughput
actually getting WORSE with more threads. Why doesn't more parallelism help a single-mutex
container, and what would you actually do about it?"** A single mutex makes the critical
section serial by construction, every pop, from every thread, funnels through the same
lock, so additional threads can only add handoff cost (cache-line ping-pong, and eventually
the mutex's park/unpark machinery once contention gets heavy enough that spinning stops
paying off), never additional throughput. Fixing it means changing the *structure*, not
tuning the lock: sharding the stack across multiple independently-locked sub-stacks, or
moving to a lock-free design (see [[015-treiber-stack]]), later chapters' territory, not
something achievable by optimizing this class's current shape.

**"A caller genuinely needs 'pop two items and push their sum' as one atomic operation,
your class only offers single-operation atomicity. How do you support this without
reinventing the interface race?"** You can't fix it by adding a bespoke method per caller
need, that's a losing race against every future caller's own multi-step requirement.
The honest answer is to expose the lock itself, or a `with_lock(fn)` method that runs an
arbitrary callback under the mutex, so composite operations become the *caller's*
responsibility under a lock the class already owns, trading some encapsulation for
genuinely supporting arbitrary composed operations, rather than pretending the class can
anticipate every composition in advance.
