# Exceptions Across Thread Boundaries

## ELI5: a courier who must call you if the delivery goes wrong

You send a courier off to deliver a package, and you get on with your day. Two ways this
can end: they deliver it fine, or something goes wrong, they trip, the package breaks,
whatever. In the second case, the courier absolutely must call *you* and tell you what
happened. If instead the courier just vanishes off the map the moment something goes
wrong, no delivery, no call, nothing, that's not "handling the problem," that's the
courier company itself collapsing because nobody was told how to report a failure.

That vanishing act is exactly what a raw `std::thread` does: if the function running on
it throws and nobody catches it right there, the whole process calls `std::terminate()`
and the entire program dies, not just the one delivery, the whole company. This question
is about building the "call me if it goes wrong" mechanism yourself.

## What you're actually building

`run_task`, run a callable on a new thread and hand the caller a `std::future` for its
result:

```cpp
template <typename F>
std::future<std::invoke_result_t<F>> run_task(F f);
```

## Requirements

1. `f` runs on a thread of its own; `run_task` itself does not block, you send the
 courier off and immediately go back to your day.
2. If `f` returns a value, `future::get()` returns it. `F` may return `void`.
3. **If `f` throws, the exception must come back out of `future::get()`, as the same
 exception, not swallowed, not replaced, and above all not `std::terminate()`.** The
 courier calls you back with the real story of what went wrong, not a generic "something
 happened" and not silence.
4. The caller never touches a `std::thread` object directly and never joins anything. The
 future is the only handle it gets, you don't personally track the courier's van, you
 just wait for the one phone call.
5. Many calls to `run_task` may be in flight at once, and their results/exceptions must
 not cross-talk, many couriers out at once, each one's report only ever reaches the
 person who sent *them*.

## Why the constraints exist

- **`F` is nullary, it takes no arguments.** No argument-forwarding to design around;
 the point of this exercise is exception propagation, not perfect forwarding.
- **Build this on `std::thread` and `std::promise`, not `std::async`.** `std::async`
 gets you this behavior for free, which is exactly why it wouldn't teach you anything
 here.
