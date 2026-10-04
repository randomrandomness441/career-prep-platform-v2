### How it's called

```cpp
SharedComputation<long long> sc([]{
    expensive_work();
    return 42LL;
});   // work() starts running in the background here, immediately

std::vector<std::thread> readers;
for (int i = 0; i < 20; ++i)
    readers.emplace_back([&sc]{ long long v = sc.get(); /* v == 42, every reader */ });
for (auto& t : readers) t.join();
// the work ran exactly once, regardless of how many readers or when they called get()
```
