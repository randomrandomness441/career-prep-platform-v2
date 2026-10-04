### How it's called

```cpp
LockFreeHashMap<int, std::string> map(64);   // 64 buckets

std::vector<std::thread> ts;
for (int t = 0; t < 8; ++t) {
    ts.emplace_back([&map, t]{
        for (int k = 0; k < 100; ++k)
            map.insert(t * 100 + k, "value" + std::to_string(k));
    });
}
// a race where several threads insert the SAME key concurrently:
std::atomic<int> winners{0};
std::vector<std::thread> racers;
for (int t = 0; t < 8; ++t)
    racers.emplace_back([&]{ if (map.insert(42, "first")) winners.fetch_add(1); });

for (auto& th : ts) th.join();
for (auto& th : racers) th.join();
// winners.load() == 1 -- exactly one insert(42, ...) call returned true

std::string v;
if (map.find(42, &v)) { /* v == "first" */ }
```
