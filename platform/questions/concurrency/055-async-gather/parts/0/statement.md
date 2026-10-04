# Async Gather: Composing Futures in Order

## ELI5: texting five friends, and writing down their answers in the order you asked

You text five friends the same question, all at once, not one at a time. They each
reply whenever they get around to it, in whatever order they happen to see their phone.
You want to write down all five answers in a list, but in the order you *asked* them,
not the order they *replied* in, because friend 3's answer needs to go in slot 3 no
matter that friend 5 happened to text back first.

## What you're actually building

Several independent async tasks are already in flight, each a `std::future<T>`. Write a
function that waits for all of them and hands back their results as one collection, the
building block behind "fan out N requests, then do something with all the answers."

```cpp
template <typename T>
std::vector<T> gather(std::vector<std::future<T>> futures);
```

## Requirements

1. `gather` returns once every future has produced a result (or thrown).
2. **The result vector must be in the same order as the input `futures` vector**,
 position `i` of the result corresponds to `futures[i]`'s task, regardless of which
 underlying task actually finishes first in real time. Friend 3's answer, in slot 3.
3. If any task threw an exception, that exception propagates out of `gather` (after
 every future has been waited on) rather than being silently dropped.

## Why the constraints exist

**No polling loop, no busy-waiting, use what `std::future` already gives you.** You're
not supposed to sit there repeatedly checking your phone; `future::get()` already blocks
until the answer is ready.
