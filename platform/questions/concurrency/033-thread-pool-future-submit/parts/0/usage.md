### How it's called

```cpp
ThreadPool pool(8);   // 8 workers, created once

std::future<int> f1 = pool.submit([]{ return 2 + 2; });
std::future<int> f2 = pool.submit([](int a, int b){ return a * b; }, 6, 7);
// f1.get() == 4, f2.get() == 42

std::future<int> f3 = pool.submit([]() -> int { throw std::runtime_error("boom"); });
try { f3.get(); } catch (const std::runtime_error&) { /* comes back through get() */ }

std::vector<std::future<int>> fs;
for (int i = 0; i < 200; ++i)
    fs.push_back(pool.submit([](int x){ return x * x; }, i % 17));
for (auto& f : fs) f.get();

// pool's destructor here: every already-queued task still runs to completion first
```
