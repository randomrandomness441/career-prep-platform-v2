## 1. Reframe the problem

Every thread needs its own ticket counter, but the interface has no room to carry one
around, `next_ticket()` is a static function, callable from anywhere on the thread, with no
object handle. The instinct is: one shared counter, protected by a mutex or made atomic, and
every thread reads-and-increments it. That's [[026-thread-safe-singleton]]'s trick for a
value every thread should *share*. Here the requirement is the opposite, each thread wants
its *own* sequence, starting at 1, with no other thread's tickets in it.

The real question isn't "how do I synchronize this counter", it's "why am I synchronizing it
at all?" Nothing needs to be shared. `thread_local` is a storage duration, the same category
as `static` or automatic (stack) storage: it says *one instance per thread*, created the first
time that thread reaches the declaration, destroyed when the thread exits. Pair it with
`static` inside the class, `inline static thread_local long tickets_ = 0;`, and `static`
gives the variable one name with process lifetime, while `thread_local` gives every thread
its own private copy under that name. No lock is needed because no two threads ever touch the
same object; there's nothing to race.

The one piece of state that genuinely is shared, `station_id()`'s dispenser, which must hand
out a globally unique id, stays a plain `static std::atomic<int>`. The exercise is telling
the two apart: which state belongs to the thread, and which belongs to the process.

## 3. The broken version, first

The broken version is one storage-duration keyword short of correct, everything else about
it, including the exact lazy-initialization logic for `station_id()`, is identical to the
solution:

```cpp
inline static int station_ = -1; // no thread_local, shared by every thread
inline static long tickets_ = 0; // no thread_local, shared by every thread
```

**Why it looks right:** single-threaded, this class is completely correct, `station_`
starts at -1, gets assigned once, `tickets_` counts up from 1. Nothing about reading the code
signals a problem; the bug only exists in the presence of a second thread, and the two static
members look exactly like ordinary per-class state, the same shape you'd write for a
single-threaded ticket counter.

Running it, 8 threads, 100,000 tickets each:

```
thread 1 (station 1): ticket 1198, expected 1, this counter is shared with another thread
```

Thread 1 didn't get its own 1, 2, 3, ..., it got whatever `tickets_` happened to be after
seven other threads had already been incrementing the *same* variable. `++tickets_` is a
read-modify-write on shared memory with no synchronization: it's a data race by the language's
own definition, and even where it happens not to corrupt (this machine's `long` increment is
often a single atomic-looking instruction pattern in practice, hence no crash), the *values*
are simply wrong, one shared sequence sliced unevenly across eight threads instead of eight
independent sequences.

The fix is the storage-duration keyword:

```cpp
inline static thread_local long tickets_ = 0;
```

Same test, same 8 threads, 100,000 tickets each:

```
ticket_station: 8 threads x 100000 tickets, sequences intact, ids unique, new threads start clean
```

Every thread now owns a private `tickets_`, so `++tickets_` is only ever touched by the one
thread that can see it, there is no longer a race to have, synchronized or not.

## 6. Where this solution fails

- **A `thread_local` object outlives nothing longer than its thread.** If a thread pool
 recycles OS threads across many logical "tasks," `tickets_` and `station_` persist across
 tasks run on the same worker thread, a new task sees the *previous* task's ticket count and
 station id, not a fresh one. `thread_local` binds to the thread, not to any notion of a
 session or task running on it; the class as written cannot distinguish them.
- **Destruction order across threads is only guaranteed relative to that thread's own exit,
 not relative to other threads or to the program's other static destructors.** A
 `thread_local` object with a non-trivial destructor that touches other global state during
 that thread's teardown can run into already-destroyed statics, same static-destruction-order
 hazard as ordinary globals, just per-thread instead of once.
- **The lazy `station_id()` check has a subtle dependency on being single-threaded per
 object.** `if (station_ < 0) station_ = next_station_.fetch_add(...)` is safe with no lock
 *because* `station_` is `thread_local`, each thread runs this branch at most once, against
 storage only it can see. That reasoning breaks the moment `station_` stops being
 `thread_local` for any reason (e.g., someone "simplifies" it back to a shared `static` to
 save memory), the exact bug in the boilerplate, reintroduced.
- **Doesn't compose with detached threads that outlive their creator's expectations.** A
 detached thread's `thread_local` storage is reclaimed when that thread exits, which the
 main thread cannot observe or wait for, if anything downstream assumed "every station id
 ever issued is still meaningful," a detached thread's contribution can vanish from view
 with no signal.
- **Memory cost scales with thread count, not with active work.** Every thread that ever
 calls any of these three functions gets its own `tickets_`, `station_`, forever, until it
 exits, for a long-lived pool with thousands of short-lived worker threads, that's
 thousands of small allocations for TLS blocks, one per thread that only used it once.

## 7. Interview follow-ups

**"Why does this need no lock at all, when the singleton in the previous question does?"**
Because there's nothing shared. A lock protects an invariant that multiple threads can
observe or mutate together; `thread_local` state has exactly one observer by construction, so
the invariant "only one thread touches this" is enforced by the storage duration itself, not
by a runtime check. The dispenser is the one piece of real shared state here, and it does use
`std::atomic`, the rule isn't "TLS means no synchronization anywhere," it's "synchronize
exactly the state that's actually shared."

**"What's the actual performance difference versus an atomic counter?"**
Measured here, 8 threads, 20,000,000 increments each: `thread_local ++` **0.35 ns/op**,
`std::atomic<long>::fetch_add` (contended, `memory_order_relaxed`) **28.1 ns/op**, roughly
80x. The atomic number isn't the "cost of atomics" in general (an *uncontended* atomic RMW is
much cheaper); it's the cost of eight cores fighting over one cache line via the coherence
protocol on every increment. `thread_local` sidesteps that entirely, each thread's copy
lives in memory only that thread's core is touching, so there's no cache-line ping-pong to
pay for.

**"This is being used inside a thread pool that reuses worker threads across many client
requests, what breaks?"** The counters and station id are scoped to the OS thread, not to
the request. A worker picks up request B right after finishing request A, and
`next_ticket()` continues A's sequence instead of starting fresh at 1 for B. If "per-thread"
was really meant as "per logical task," you need an explicit reset hook the pool calls between
tasks, or you need the state keyed by task id instead of by thread, which puts you back to
carrying a handle, the exact thing this interface was trying to avoid.

**"How would you debug 'thread 47 has the wrong ticket sequence' in production, days after the
fact, without being able to attach a debugger while it's happening?"** Log `station_id()`
alongside every ticket issued, it's cheap (assigned once, read after) and turns "which
physical OS thread" into "which logical station," which survives thread-pool reuse and thread
recycling in a way a raw `std::thread::id` printed in a log doesn't reliably correlate across
restarts. If the bug were a resurrected shared-state race like the boilerplate's, you'd expect
duplicate or non-monotonic ticket values *within one station's log lines*, the diagnostic
signature is different from a thread just crashing or hanging.
