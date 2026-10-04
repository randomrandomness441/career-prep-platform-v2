### How it's called

```cpp
std::vector<int> in = {5, 3, 8, 1, 9, 2, /* ... hundreds of thousands more ... */};

std::vector<int> sorted = parallel_quick_sort(in);
// in itself is untouched; sorted == a sorted copy of in

// also called with instrumented element types that record which thread
// performed each comparison, and types whose comparison can throw:
std::vector<Probe> got = parallel_quick_sort(probes);   // Probe::operator< logs its thread id
std::vector<Bomb>  got2 = parallel_quick_sort(bombs);   // Bomb::operator< throws sometimes
```

No threads are visible to the caller — it's a single call that returns a fully sorted
vector, internally spreading the work across threads it creates and joins/gets itself.
