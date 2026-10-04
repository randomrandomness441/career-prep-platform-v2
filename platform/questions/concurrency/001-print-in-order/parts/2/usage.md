### How it's called

One `Ordered` shared by N threads, each calling `go` with its own id exactly once. The
threads are started in a shuffled order — `go(1, ...)` might be the last one launched.

```cpp
const int N = 100;
Ordered ord(N);

std::vector<std::thread> ts;
for (int id : shuffled_ids_from_1_to_N) {
    ts.emplace_back([&ord, id]{
        ord.go(id, [id]{ std::cout << id << ' '; });
    });
}
for (auto& t : ts) t.join();
// output is always "1 2 3 ... 100", regardless of launch order
```

Each thread calls `go` exactly once, with a distinct `id` in `[1, N]`. The callback for
`id` must not run until every callback for `1..id-1` has already finished.
