### How it's called

```cpp
Watchdog wd;

bool ok1 = wd.run_with_deadline([]{ do_quick_thing(); }, std::chrono::milliseconds(500));
// ok1 == true, returns as soon as do_quick_thing() finishes -- well under 500ms

bool ok2 = wd.run_with_deadline([]{
    std::mutex a, b;               // deliberately deadlocks with another thread
    std::lock_guard<std::mutex> g1(a);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::lock_guard<std::mutex> g2(b);   // never acquired -- the other thread holds it
}, std::chrono::milliseconds(200));
// ok2 == false, returned at ~200ms even though the lambda's thread never finishes
```

Each call to `run_with_deadline` is independent; the caller doesn't manage any threads of
its own.
