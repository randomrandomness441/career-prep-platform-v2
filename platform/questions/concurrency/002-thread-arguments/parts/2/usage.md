### How it's called

```cpp
std::atomic<int> out{-1};
auto job = std::make_unique<Job>(Job{42});

std::thread t = run_job(std::move(job), &out);   // job transferred into the thread
// `job` is now empty (nullptr) -- ownership moved, not shared
t.join();
// out.load() must be 42
```
