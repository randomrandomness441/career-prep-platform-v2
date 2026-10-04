## 1. Reframe the problem

Locking one mutex is easy. Locking **two** is where deadlock lives.

The moment an operation needs two locks at once, a second thread can be part-way through
the same operation in the opposite direction. Thread 1 holds A and wants B. Thread 2 holds
B and wants A. Neither will ever let go, because each is blocked *while holding* what the
other needs.

Here's what's not the problem: every individual access is correctly protected. The naive
code has no data race. It's perfectly synchronized and completely stuck. **Correct locking
of each piece does not give you a correct program.** The failure is in the *order* locks
get acquired, and that's a property of the whole system, not any one function.

So the real question is: how do you take two locks so that no ordering of callers can
produce a cycle?

## 2. The tools

### The problem, minimally

```cpp
bool transfer(Account& from, Account& to, long amount) {
    std::lock_guard<std::mutex> a(from.m); // thread 1 gets A here
    std::lock_guard<std::mutex> b(to.m); // ...and blocks here forever
    ...
}
```

`transfer(A, B, ...)` and `transfer(B, A, ...)` acquire in opposite orders. That's the
cycle.

### `std::scoped_lock` (C++17), the fix

```cpp
std::scoped_lock lk(from.m, to.m); // locks BOTH, deadlock-free
```

One statement, any number of mutexes, released in reverse order at scope exit.

Here's how it avoids deadlock. It does **not** simply lock them in the order given. It
blocks on one, then `try_lock`s the others. If any `try_lock` fails, it **releases
everything it holds** and restarts, this time blocking on the one that refused. It never
sits holding one lock while blocking on another, so the hold-and-wait condition never
occurs. No cycle can form, no matter what order different callers pass their arguments in.

The pre-C++17 spelling, still common in older code:

```cpp
std::lock(from.m, to.m); // same algorithm
std::lock_guard<std::mutex> a(from.m, std::adopt_lock); // adopt = already locked
std::lock_guard<std::mutex> b(to.m, std::adopt_lock);
```

`std::scoped_lock` is that, with the RAII wired in for you.

### The alternative: consistent ordering

Deadlock needs a cycle, and a cycle needs inconsistent order. So you can also just impose a
global order that every caller obeys:

```cpp
std::mutex* first = &from.m < &to.m ? &from.m : &to.m;
std::mutex* second = &from.m < &to.m ? &to.m : &from.m;
std::lock_guard<std::mutex> a(*first);
std::lock_guard<std::mutex> b(*second);
```

Ordering by address is arbitrary, but it's consistent, and that's all that's required. This
scales to any number of locks and costs nothing at runtime. But it relies on **every** site
obeying the convention. `scoped_lock` is local and can't be forgotten. Prefer it. Reach for
explicit ordering when the locks get taken across functions that can't see each other.

### The `from == to` trap

```cpp
transfer(a, a, 10);
```

Locking one `std::mutex` twice on the same thread is undefined behavior. In practice,
that's an immediate self-deadlock. `std::scoped_lock` and `std::lock` both *require* the
mutexes to be distinct, so they don't save you here. You need an explicit check. That's why
the solution starts with `if (&from == &to) return true;`.

## 3. The broken version, first

```cpp
std::lock_guard<std::mutex> a(from.m);
std::lock_guard<std::mutex> b(to.m);
```

Two threads, 500 transfers each, in opposite directions. Actual result:

```
after 2 seconds: 0 of 1000 transfers completed
STUCK, thread 1 holds A waiting for B; thread 2 holds B waiting for A.
```

**Zero.** Not "slower", not "occasionally wrong". The program stops existing as a useful
process within microseconds and never recovers.

Two things about this failure are worth remembering:

- **The code looks obviously correct.** Lock both accounts, move the money, unlock. Every
 reviewer nods. There's nothing to see in the diff.
- **It's timing-dependent, so testing misses it.** The demo above inserts a 50µs sleep
 between the two locks to make it fire every run. Without that sleep, the window is a
 handful of nanoseconds. You can run this a million times in CI and never hit it, then
 deadlock in production the first time the machine is under load. That's why the
 platform's stress stage injects scheduling delays. It manufactures the window your
 laptop won't give you.

## 4. Real-world usage

**Why `std::lock` exists.** Deadlock avoidance was one of the first things practitioners
demanded after C++11 shipped threads. The bank-transfer shape, an operation that touches
two independently-locked objects, is everywhere: transferring between accounts, moving an
item between two containers, swapping two objects, updating both ends of a graph edge.
Every one of them is this bug. `std::lock` (C++11) and later `std::scoped_lock` (C++17)
exist so the correct thing is one line instead of a hand-written back-off loop.

**Where you meet it in production:**

- **Anything named `transfer`, `move`, `swap`, or `merge`.** Two objects, two locks.
- **Graph and tree structures**, where an operation touches a node and its neighbour, or a
 parent and child. Hand-over-hand traversal has exactly this hazard.
- **`std::swap` on locked types.** A correct thread-safe `swap` has to lock both sides, and
 it's the textbook `scoped_lock` use case.

**Where NOT to use it:**

- **Don't use it for one mutex.** `std::scoped_lock lk(m)` works, but `std::lock_guard`
 states the intent better. (`std::scoped_lock lk;` with *no* arguments locks nothing. An
 easy typo that silently removes all protection.)
- **Don't reach for it when you can avoid the second lock entirely.** The fastest fix for a
 two-lock deadlock is often to not need two locks. Use one coarser mutex, or restructure so
 the operation touches one object. Taking two locks is a design smell before it's a
 correctness problem.
- **It doesn't help across unrelated call sites.** If function X takes lock A then calls
 into function Y which takes lock B, `scoped_lock` can't see both. That needs a lock
 *hierarchy* enforced at runtime, which is the next question.
- **It requires distinct mutexes.** Same-object calls need the explicit guard shown above.

## 5. Performance

Uncontended, 5 million iterations:

| | ns per operation |
|---|---|
| two nested `lock_guard`s | 13.5 |
| `std::scoped_lock(a, b)` | **13.2** |

**Deadlock avoidance is free.** In the uncontended case, which is the overwhelmingly common
one, `scoped_lock` compiles to essentially the same thing as locking both in order. There's
no fast-but-dangerous version to trade against. The back-off algorithm only does extra work
when a `try_lock` actually fails, and that's exactly when the naive version would have
deadlocked.

So there's no performance argument for the naive form. It's slower *and* broken, or at best
equal *and* broken.

Under heavy contention `scoped_lock` can do more work than a fixed ordering, because a
failed `try_lock` throws away progress and restarts. If you measure that as a bottleneck,
address-ordering is the alternative. But measure first.

## 6. Where this solution fails

- **`from == to` is undefined behavior without the explicit check.** `scoped_lock` doesn't
 detect it. A transfer of money to the same account is a perfectly reasonable input from a
 user, and it hangs the thread.
- **It only protects locks taken together.** Deadlock across call boundaries, where X holds
 A and calls Y which wants B while Z holds B and calls W which wants A, is invisible to
 `scoped_lock`. You need a lock hierarchy for that.
- **Holding locks while calling unknown code.** If the code inside the critical section
 calls a user-supplied callback, that callback can take more locks and reintroduce a cycle.
 Never call out to arbitrary code while holding a lock.
- **Lock convoying under contention.** Correct but slow: with many threads transferring
 between a small set of hot accounts, they serialize on the same mutex pair and throughput
 collapses. The fix is sharding or a lock-free design, not a better lock.
- **The check-then-act inside is only safe because both locks are held.** `if (from.balance_
 < amount)` followed by the subtraction is atomic *only* within the critical section. Move
 either line outside and the balance can go negative. That's the same class of interface
 race as the previous question.

## 7. Interview follow-ups

**"How exactly does scoped_lock avoid the deadlock? Walk through the algorithm, not just
the guarantee."** It doesn't lock the mutexes in the order given and hope. It blocks on the
first one, then attempts `try_lock` on the rest. If any of those `try_lock` calls fails,
meaning some other thread got there first, it releases *everything it currently holds* and
restarts, this time blocking on whichever mutex refused it. It's never sitting holding one
lock while blocking indefinitely on another, so hold-and-wait, one of the four necessary
conditions for deadlock, can never occur. It doesn't matter what order different callers
pass their two mutexes in.

**"You measured scoped_lock as free when uncontended. Is that always true, or does it
degrade under contention?"** Uncontended, yes, essentially free, matching two nested
`lock_guard`s. Under real contention it can do *more* work than a fixed address-ordering
scheme, specifically because a failed `try_lock` throws away whatever partial progress it
made and restarts from scratch, repeatedly, if contention is heavy enough. The reading's
own advice stands: measure before assuming either approach is faster under load. Don't
extrapolate the uncontended number to the contended case.

**"Small machine, 1-2 cores. Does the two-mutex deadlock still happen, and does the
back-off algorithm still make sense there?"** The deadlock is a scheduling property, not a
parallelism one. Even one core, time-slicing between two threads, can preempt thread 1
right after it locks A and before it locks B, then run thread 2 far enough to lock B and
block on A. Nothing changes about correctness. The `scoped_lock` back-off algorithm still
applies, though on a single core it's the OS's own scheduling, not genuinely simultaneous
`try_lock` attempts on different cores, that decides whether the first attempt succeeds or
has to restart.

**"transfer(a, a, 10), same account both sides. Why doesn't scoped_lock save you here, and
how would you actually test for this in code review or CI?"** `std::scoped_lock` and
`std::lock` both require the mutexes passed to be distinct objects. Locking the same
`std::mutex` twice on one thread is undefined behavior no matter which locking primitive
attempts it, typically an immediate self-deadlock in practice. This needs an explicit
guard, `if (&from == &to) return ...;`, before any locking happens at all. It's not
something the locking primitive can detect or protect you from, since by the time
`scoped_lock` runs it has no way to know the two references alias the same object versus
two distinct ones with equal values. A property-based test that specifically includes
same-account transfers as an input case, not just "two different accounts, both orderings",
is how you'd catch this in CI instead of by inspection.

**"Production is on fire. A two-mutex deadlock made it past testing and is now hanging live.
How would you diagnose it without a debugger attached ahead of time?"** Attach and dump
every thread's stack: `gdb -p <pid>`, `thread apply all bt`, or the platform's equivalent.
A genuine deadlock shows as two or more threads each blocked inside a mutex `lock()` call.
Cross-referencing which mutex each is waiting for against which mutex each already holds,
visible right in the stack trace's calling context, reveals the cycle directly. This is the
same diagnostic technique [[041-deadlock-detection-watchdogs]]'s reading describes for its
own case. A live stack dump is the standard first move for any suspected deadlock, not just
this specific bank-transfer shape.
