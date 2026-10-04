## 1. Reframe the problem

`std::future<T>` was designed around a picture with exactly one consumer: one thread launches
work, one thread, maybe the same one, maybe not, eventually asks for the answer. That
picture is baked into the type, not just the documentation. `get()` **moves** the result out
of the shared state, the way `std::unique_ptr::release()` hands over ownership. Ownership can
only be handed over once. Ask a `unique_ptr` for `release()` twice and the second call gets
nothing, because there is nothing left to give, `future::get()` is the same idea, just for a
value that hasn't arrived yet instead of a pointer that has.

The problem this question asks is: what happens the moment that picture stops being true,
when the result of one computation is needed by *several* independent readers, each of whom
wants to call something that looks like `get()`? You cannot copy a `std::future` (ownership
again, two owners of one moved-from value makes no sense), and you cannot safely call `get()`
on the same `future` object from more than one thread even once each, because the standard
only promises `future`'s member functions are safe when exactly one thread is touching the
object.

So this is not "how do I read a value", reading an already-computed value from many threads
at once is the easiest thing in concurrent programming, because there is nothing to
synchronise once nobody is writing any more. It's "which type actually represents 'an
immutable value that shows up later and then never changes,'" because `future` is not that
type. Something else in `<future>` is.

## 3. The broken version, first

The naive version keeps a `std::future<T>` as the member and calls `.get()` on it. It looks
finished:

```cpp
template <typename T>
class SharedComputation {
    std::future<T> fut_;
public:
    explicit SharedComputation(std::function<T()> work)
    : fut_(std::async(std::launch::async, std::move(work))) {}
    T get() { return fut_.get(); }
};
```

**Why this looks right.** `std::async` starts the work in the background, check. `get()`
blocks until it's ready and returns the value, check. Nothing here mutates any shared state
that another thread could be racing on; `get()` reads an already-finished result. If you've
internalised "concurrent reads of an immutable value are always safe", which is generally
true, this code has nothing that looks like a bug. It compiles clean, runs clean the first
time, and returns the right number.

**What actually happens**, running the boilerplate's own class, calling `get()` twice in a
row on one thread, no second thread anywhere in the program:

```
first get(): 123456789
```

...and the process is gone. Exit code 139, killed by `SIGSEGV`. No exception, no error
message, no second line of output. Ten runs in a row: ten segfaults, same way every time.

**Why it's not "probably fine, most code only calls get() once."** The whole point of this
question is that *fan-out* code calls `get()` more than once by construction, a second
reader, a retry, a logging path that also wants the value. The naive class *type-checks*
against a fan-out use, compiles, and runs perfectly for exactly the first caller. The second
caller, which could be a second thread, or could just be your own code calling `get()` again
five minutes later, gets a crash with no diagnostic pointing at the real mistake, which was
made at the type-choice level, one commit before.

**The fix** is one word: `std::shared_future<T>` instead of `std::future<T>`, built by calling
`.share()` on the future `std::async` hands back. `shared_future::get()` is `const`, it reads
an already-published value out of shared state it does not own exclusively, rather than moving
that value out and invalidating the state behind it. That's the whole difference: `future`
represents "the one-time transfer of a result to its one owner"; `shared_future` represents
"a value that has arrived and now simply *is*, for as many readers as want it, as many times
as they want to look."

## 6. Where this solution fails

- **`work` must not have side effects a caller can rely on ordering.** All readers see the
 same value, but nothing about `shared_future` tells you *when* `work` ran relative to
 something else in your program besides "before the first successful `get()` anywhere."
 If two `SharedComputation`s need to run in a specific order relative to each other, this
 type doesn't express that, you need an explicit dependency, not two independent futures.
- **An exception from `work` is also broadcast, and also only readable once conceptually
 per catch, but every reader gets to catch it.** `shared_future::get()` rethrows the stored
 exception on every call, from every reader. That's usually what you want (everyone learns
 the computation failed), but it means N readers all pay the cost of formatting/handling the
 same exception, which is easy to forget if you only tested the success path.
- **This is not a substitute for a cache with a key.** `SharedComputation<T>` broadcasts *one*
 precomputed value. The moment you need "the result for input X, or Y, or Z, each computed
 once and shared," you need a map from key to `SharedComputation<T>` (or `shared_future<T>`)
 plus a mutex around inserting new keys, this class is the unit you'd put inside that map,
 not a replacement for it.
- **Destroying a `SharedComputation` whose work hasn't finished still blocks.** The
 `shared_future` came from `std::async(std::launch::async, ...)`; the *original* `std::future`
 that `.share()` was called on inherits `std::async`'s special rule that its destructor
 blocks until the task finishes if it's the last reference and the task hasn't been waited
 on, in practice here that mostly resolves before the `.share()` call returns, but it means
 you cannot rely on destroying an in-flight `SharedComputation` to cancel or detach the work.
 There is no cancellation here at all: once constructed, `work` runs to completion no matter
 what.
- **Copying is cheap on purpose, and that can surprise you.** `shared_future` is
 copy-constructible specifically so many holders can each keep their own handle to the same
 state. If `T` itself is expensive to copy, every `get()` call pays a copy of the *value*
 (or you return `const T&` and manage the lifetime yourself), the "share the future" part is
 free, the "share the value" part is exactly as expensive as copying `T` always was.

## 7. Interview follow-ups

**"Why does the standard even provide a single-use `future`, why not make every future
shared by default?"** Cost and clarity of ownership. A plain `future`'s shared state can be
torn down the instant its one owner has read it; `shared_future` has to keep the state alive
and manage a reference count for however many holders exist, which is real (if usually small)
overhead. More importantly, "exactly one place consumes this" is a very common shape, a
worker returning a result to its caller, and a type that enforces that at compile time
(you can't accidentally hand the same result to two places without explicitly opting in via
`.share()`) documents the intended ownership instead of hoping the reader guesses it.

**"You've got a `std::promise<T>` upstream instead of `std::async`, does anything change?"**
No. `promise::get_future()` returns a plain `future<T>`, and you `.share()` it the same way;
the promise/future split is orthogonal to single-use-vs-shared. Whoever calls `promise::set_
value` still only calls it once, that part of the contract is unrelated to how many readers
the *result* has.

**"Small machine, one core, everything time-sliced. Does the crash still happen?"** Yes,
identically. The bug in the naive version isn't a race that needs two threads running at once
to manifest, the single-threaded reproduction above proves that: the second `get()` call is
already-invalid-state misuse regardless of scheduling. Concurrency makes the *consequences*
worse (more callers hit it, in less predictable order) but core count has nothing to do with
whether the bug exists.

**"Extreme load, 10,000 concurrent readers instead of 12. What actually changes in the
correct version?"** Almost nothing algorithmically: `shared_future::get()` is a read of
already-settled state, so 10,000 threads calling it concurrently is 10,000 independent, wait-free
reads of the same cache lines, there's no lock to contend on. What changes is memory
traffic (10,000 cores' worth of cache lines all wanting to read the same shared-state block)
and, if `T` is large, 10,000 copies of it, that's a real cost, and the fix is usually to
return `const T&` from a wrapper instead of a by-value copy, and to size `T` deliberately.

**"Production ops: how would you tell 'the broadcast never arrived' from 'the broadcast is
just slow,' from monitoring alone?"** `shared_future::get()` blocks with no timeout by
default, you'd want `wait_for` with a deadline at the call site, log/alert when it fires
before the future is ready, and treat "still not ready after N seconds" as its own incident
class distinct from "returned, but wrong" or "threw." That's a different, narrower problem
than this question, it's exactly what the deadlock/watchdog question in this course covers:
detecting a stall in an operation you expected to finish.
