# Fan-Out with shared_future

## ELI5: one exam, graded once, checked by the whole class

A teacher grades one big exam, that's expensive, it takes a while, and it only needs to
happen *once*. Every student in the class wants to know their result, and they'll each
check the results board whenever they get around to it, maybe several times, maybe all
at the exact same moment as everyone rushing the board after the bell rings. The grading
absolutely must not happen 30 separate times, once per student who asks. It happens once,
in the background, and every single student, no matter when or how many times they
check, reads the *same* finished result off the same board.

## What you're actually building

An expensive computation, parsing a config blob, warming a cache, whatever, has exactly
one result, and several downstream consumers all need to read it once it's ready. Nobody
wants to redo the work per consumer, and nobody wants to hand-roll their own "is it ready
yet" flag with a mutex and a condition variable.

Write `SharedComputation<T>`:

```cpp
template <typename T>
class SharedComputation {
public:
    explicit SharedComputation(std::function<T()> work); // starts work() asynchronously, once
    T get() const; // waits for the result if necessary, then returns it
};
```

`work` runs exactly once, on some other thread, no matter how many times `get()` is
called or how many threads call it. Every caller, from any thread, any number of times,
including concurrently with other callers, must see the same, correct result.

## Why the constraints exist

- **`work` must actually run in the background (asynchronously)**, not on the calling
 thread the first time someone calls `get()`. Grading doesn't happen the instant the
 first student walks up to check, it's already running before anyone asks.
- **`get()` must be safe to call multiple times, from multiple threads, at the same
 time, on the same `SharedComputation` instance.** The whole class rushing the board at
 once is the normal case, not an edge case.
- `T` is a small, copyable type (the tests use `long long` and a small struct).

Nothing here needs a `mutex` or a `condition_variable` that you write yourself, the
standard library already has the tool for "one write, many concurrent reads" of an
asynchronous result. Find it.
