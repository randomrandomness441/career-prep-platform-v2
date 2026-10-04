### How it's called

```cpp
StreamCounter counter(8);   // 8 independent shards

std::vector<std::thread> ts;
for (int i = 0; i < 8; ++i)
    ts.emplace_back([&counter]{ for (int n = 0; n < 1'000'000; ++n) counter.increment(); });
for (auto& t : ts) t.join();

// counter.total() == 8'000'000, exactly, every run
```
