### How it's called

```cpp
std::vector<int> v(2501, 1);   // 2501 elements, each 1

long long total = parallel_accumulate(v, 12345LL);
// total == 12345 + 2501, computed by splitting v into plan_threads(2501) blocks

std::vector<int> empty;
long long z = parallel_accumulate(empty, 100LL);
// z == 100 -- no thread created at all
```

A single call, internally spreading the summation across `plan_threads(v.size())`
threads it creates and joins itself (or none at all, for a small `v`).
