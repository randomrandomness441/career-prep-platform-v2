### How it's called

```cpp
std::vector<long> input = {3, 1, 4, 1, 5, 9, 2, 6};

std::vector<long> result = parallel_prefix_sum(input, /*num_threads=*/8);
// result == {0, 3, 4, 8, 9, 14, 23, 25}
```

A single call, internally spreading the per-chunk sums and the offset handoff across
`num_threads` threads it creates and joins itself.
