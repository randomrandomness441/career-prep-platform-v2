# How Many Threads?

## ELI5: don't hire a mover for every single box

You're moving a huge pile of boxes and want to hire help. Hiring one mover per *box*
would be absurd, you'd spend more time interviewing and paying movers than the move
itself takes, and most of them would show up just to carry one box and leave. Hiring
*zero* movers for a pile of 3 boxes is also silly, just carry them yourself, faster than
finding anyone. The actual skill is deciding, based on the size of the pile, how many
movers is actually worth it: enough that the work gets split up meaningfully, not so many
that hiring overhead swamps the job.

## What you're actually building

Write a parallel sum that decides *at runtime* how many threads to use.

```cpp
std::size_t plan_threads(std::size_t length);

template <typename E, typename T>
T parallel_accumulate(const std::vector<E>& v, T init);
```

`parallel_accumulate` must return the same value as
`std::accumulate(v.begin(), v.end(), init)`, computed by splitting `v` into
`plan_threads(v.size())` contiguous blocks. `E` and `T` may differ, summing `int`
elements into a `long long` total is a normal call.

**`plan_threads`, exact contract**

```
min_per_thread = 1000 // do not hand a thread less work than this
hw = std::thread::hardware_concurrency()
cap = (hw != 0) ? hw : 2 // 0 means "no idea"; pick something sane

plan_threads(0) == 0
plan_threads(n) == min(cap, ceil(n / min_per_thread)) for n >= 1
```

So `plan_threads(999) == 1`, `plan_threads(1000) == 1`, `plan_threads(1001) == 2`, and
no length on Earth makes it exceed `cap`, you never hire more movers than there are
hands available to use them.

## Requirements

1. `plan_threads(length) <= 1` means **no thread is created at all**, the calling
 thread does the whole sum itself. Not one worker thread it then waits for. Zero movers
 hired; you just carry the boxes yourself.
2. `parallel_accumulate` on an empty range returns `init`.
3. `init` is added exactly once, whatever the split.
4. Never create more than `plan_threads(length)` threads, counting the caller as one of
 them. Launch `n - 1` workers and do the last block yourself.
5. Every element is summed exactly once. Integer division means the last block is the
 one that absorbs the remainder.

## Why the constraints exist

- **Each worker writes to its own slot in a results vector.** That's a shared vector,
 but no two threads touch the same element, so no mutex is needed, each mover has
 their own truck.
- **Join every thread you launch.**
- **`T` is any type with `operator+`.** Do not assume it is an integer.
