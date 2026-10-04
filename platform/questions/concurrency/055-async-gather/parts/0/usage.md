### How it's called

```cpp
std::vector<std::future<int>> futures;
futures.push_back(std::async(std::launch::async, []{ sleep_random(); return 1; }));
futures.push_back(std::async(std::launch::async, []{ sleep_random(); return 2; }));
futures.push_back(std::async(std::launch::async, []{ sleep_random(); return 3; }));

std::vector<int> results = gather(std::move(futures));
// results == {1, 2, 3}, always -- in input order, regardless of finish order

std::vector<std::future<int>> f2;
f2.push_back(std::async(std::launch::async, []() -> int { throw std::runtime_error("x"); }));
try { gather(std::move(f2)); }
catch (const std::runtime_error&) { /* propagated out of gather */ }
```
