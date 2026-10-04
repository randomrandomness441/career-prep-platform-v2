Start with `plan_threads` alone, on paper. It has three moving parts and no threads in it:

```cpp
if (length == 0) return 0;
const std::size_t max_useful = (length + min_per_thread - 1) / min_per_thread;  // ceil
const unsigned hw = std::thread::hardware_concurrency();
```

`hw` may be 0. Decide what you do about that *before* you use it in an expression.
---
`num_threads <= 1` is not "one worker thread". It is **no threads**: return
`std::accumulate(v.begin(), v.end(), init)` and you are done. That single early return is
what makes the empty range, the one-element range, and the 999-element range all correct
and all free.

For the rest, the shape is: `block_size = length / num_threads`, launch `num_threads - 1`
workers on consecutive index ranges, and let the calling thread do the final block — which
is also the block that absorbs the remainder from the integer division, so it must run to
`v.end()`, not to `block_start + block_size`.
---
```cpp
std::vector<T> results(num_threads);
std::vector<std::thread> threads;
threads.reserve(num_threads - 1);

std::size_t block_start = 0;
for (std::size_t i = 0; i + 1 < num_threads; ++i) {
    const std::size_t block_end = block_start + block_size;
    threads.emplace_back([&v, block_start, block_end, &results, i] {
        results[i] = std::accumulate(v.begin() + block_start, v.begin() + block_end, T());
    });
    block_start = block_end;
}
results[num_threads - 1] = std::accumulate(v.begin() + block_start, v.end(), T());

for (auto& t : threads) t.join();
return std::accumulate(results.begin(), results.end(), init);
```

Three things to notice. `block_start`/`block_end` are captured **by value** —
`block_start` is about to be reassigned by the next loop iteration, so a reference capture
would be a race on a variable that is also changing under the worker's feet. Each worker
starts from `T()`, not from `init`; `init` is folded in exactly once at the end. And the
join loop comes *after* the caller's own block, so the caller is working rather than
waiting.
