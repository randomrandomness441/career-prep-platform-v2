### How it's called

```cpp
std::future<int> f1 = spawn_task([]{ return 42; });
int v = f1.get();                              // 42, computed off the calling thread

std::future<int> f2 = spawn_task([](int a, int b){ return a * b; }, 6, 7);
// f2.get() == 42 -- arguments forwarded through

std::future<int> f3 = spawn_task(
    [](std::unique_ptr<int> p){ return *p + 1; }, std::make_unique<int>(7));
// move-only argument works

std::future<void> f4 = spawn_task([]{ side_effect(); });
f4.get();                                       // void return works too

std::future<int> f5 = spawn_task([]() -> int { throw std::runtime_error("boom"); });
try { f5.get(); }
catch (const std::runtime_error&) { /* the ORIGINAL exception, not future_error */ }

// many in flight at once:
std::vector<std::future<int>> fs;
for (int i = 0; i < 32; ++i) fs.push_back(spawn_task([](int k){ return k * k; }, i));
for (auto& f : fs) f.get();
```

`spawn_task` never blocks the caller and hands back no thread handle — only the future.
