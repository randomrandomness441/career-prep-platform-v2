## What is a mutex?

A mutex ("mutual exclusion") is a lock that guarantees at most one thread is ever
executing the code between `lock()` and `unlock()` at a time. That's the mechanism.
What it's actually *for* is protecting an **invariant**, some fact about shared state
that has to stay true, and would get corrupted if two threads could touch it at the
same instant.

"It makes things thread-safe" is not a real answer in an interview, and this course's
own axiom 5 says so directly: **no mutex without a stated invariant.** For every lock
you reach for, you should be able to say precisely *what* it's protecting and *which*
threads could observe it broken without the lock. A few concrete examples, from this
platform's own questions:

- In [[030-atomics-vs-mutexes]]'s `HotCounter`, the invariant is trivial, "the counter's
 value reflects exactly the number of increments that have completed", and turns out
 not to need a mutex at all; a single atomic does the job, because there's only one
 piece of state and one operation on it.
- In [[005-scoped-lock]]'s bank `transfer`, the invariant is "the balance check and both
 updates happen as one indivisible step", miss that, and two transfers can both read
 a balance before either one writes it back, letting an account go negative that
 should have been rejected.
- In [[057-event-gate-dispatcher]]'s `EventGate`, the invariant is "`event_active_` and
 `pending_` always agree with each other", specifically, that no thread can observe
 the flag and act on it after that observation has gone stale.

A mutex that exists because "the reviewer said this needs a lock" without a clear
answer to "protecting what, from whom" is usually either protecting the wrong thing, or
protecting nothing a race could actually break, both are real interview red flags, and
"what invariant does this protect" is a completely fair follow-up to expect on any
mutex you add.

## What are the possible deadlock scenarios?

Every deadlock this course's questions demonstrate boils down to the same shape: two or
more threads, each holding something another one needs, each waiting for the other to
let go first, forever. The concrete forms it shows up in:

- **Lock-ordering inversion.** Thread 1 locks A then wants B; thread 2 locks B then
 wants A. Neither ever gets its second lock. [[005-scoped-lock]]'s naive bank
 `transfer` is exactly this, and [[006-hierarchical-mutex]] is the general fix, give
 every lock a level, and only ever acquire strictly decreasing levels, which makes the
 cycle above structurally impossible.
- **The dining philosophers pattern.** Every philosopher picks up their left fork first;
 with enough philosophers at the table simultaneously, everyone ends up holding one
 fork and waiting for a neighbor's, forever. [[021-dining-philosophers]] is this
 exact scenario, made concrete.
- **Holding a lock while calling into code that reacquires it.** A callback invoked
 while a mutex is still held, where that callback (directly or through several layers)
 tries to lock the same mutex again, an instant self-deadlock on a non-recursive
 `std::mutex`. [[057-event-gate-dispatcher]]'s naive version has exactly this bug: it
 runs queued callbacks while still holding its lock, and a callback that re-registers
 itself deadlocks outright.
- **Waiting on a condition that only the deadlocked thread itself can satisfy.** Less
 common, but real: a thread blocks on a condition variable waiting for state that only
 gets updated by code the same thread was supposed to run next.

The one-line diagnostic question for any of these: **can I draw a cycle?**, thread A
waits on something thread B holds, thread B waits on something thread A holds (directly,
or through a longer chain). If yes, it's a deadlock waiting to happen under the right
schedule, whether or not you've ever actually observed it hang.

## When does a "concurrent modification exception" occur?

It's the Java/C# name for a specific, very common mistake: mutating a collection (a
`List`, `Map`, etc.) while something else is iterating over it. Managed languages
usually detect this at runtime and throw a `ConcurrentModificationException` rather
than let it silently corrupt the iteration.

**C++ has no such exception.** The equivalent mistake, iterating a `std::vector` (or
any STL container) while another thread inserts into or erases from it, unsynchronized
, is undefined behavior, not a helpful diagnostic. It might crash. It might silently
skip or repeat elements. It might work fine in testing and fail once in production under
a schedule you never hit locally. This is worth stating explicitly in an interview: the
*concept* (mutating a collection out from under an in-progress iteration) is shared with
managed languages, but the *consequence* in C++ is strictly worse, because there's no
runtime safety net catching it for you.

The general fix pattern shows up throughout this course: never iterate a container that
another thread might be mutating without a lock held across the whole iteration (or a
data structure specifically designed for concurrent iteration, like
[[040-lock-free-hashmap]]'s insert-only design, which sidesteps the problem by simply
never invalidating anything once published). [[057-event-gate-dispatcher]]'s `swap`-
under-the-lock pattern, copy what you need to iterate out under the lock, then iterate
the *copy* with the lock released, is the standard shape of the fix when you also don't
want to hold a lock across arbitrary iteration bodies.
