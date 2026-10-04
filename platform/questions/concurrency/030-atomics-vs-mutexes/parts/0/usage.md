### How it's called

```cpp
HotCounter c;

std::vector<std::thread> ts;
for (int i = 0; i < 8; ++i)
    ts.emplace_back([&c]{ for (int n = 0; n < 100000; ++n) c.increment(); });
for (auto& t : ts) t.join();

// c.get() == 800000, exactly, every run
```
