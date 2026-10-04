### How it's called

```cpp
std::vector<int> values = {2, 0, 1, 2, 0, 1, 1, /* ... millions more, all in [0, 3) */};

std::vector<std::size_t> perm = parallel_argsort(values, /*num_buckets=*/3, /*num_threads=*/8);
// values[perm[0]] <= values[perm[1]] <= ... <= values[perm.back()]
// perm is a permutation of 0..values.size()-1
```

A single call, internally spreading the bucket-counting and placement work across
`num_threads` threads it creates and joins itself.
