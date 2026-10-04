### How it's called

```cpp
struct Config { Config() { /* expensive setup */ } int value = 42; };

std::vector<std::thread> ts;
for (int i = 0; i < 32; ++i)
    ts.emplace_back([]{
        Config& c = singleton<Config>::get();   // constructed exactly once, by whichever
        use(c);                                 // thread gets there first
    });
for (auto& t : ts) t.join();

// a type whose constructor throws the first N times it's called:
try { singleton<Flaky>::get(); } catch (...) { /* next call retries construction */ }
Flaky& f = singleton<Flaky>::get();   // eventually succeeds; that instance sticks
```

Many threads call `get()` on the same `T` concurrently with no locking of their own.
