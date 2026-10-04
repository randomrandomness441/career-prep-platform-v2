### How it's called

```cpp
std::future<int> f1 = run_task([]{ return 42; });
int v = f1.get();                              // 42

std::future<void> f2 = run_task([&]{ side_effect(); });
f2.get();                                       // void works too

std::future<int> f3 = run_task([]() -> int { throw std::runtime_error("boom"); });
try { f3.get(); }
catch (const std::runtime_error&) { /* the ORIGINAL exception, not std::terminate */ }

// many in flight at once, each future independent:
std::vector<std::future<int>> fs;
for (int i = 0; i < 32; ++i) fs.push_back(run_task([i]{ return i * i; }));
for (auto& f : fs) f.get();
```

`run_task` never blocks the caller and hands back no thread handle — only the future.
