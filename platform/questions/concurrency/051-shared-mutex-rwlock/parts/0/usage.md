### How it's called

```cpp
ConfigStore store;
store.set("timeout", "30");

std::vector<std::thread> readers, writers;
for (int i = 0; i < 20; ++i)
    readers.emplace_back([&]{ for (int n = 0; n < 1000; ++n) (void)store.get("timeout"); });
for (int i = 0; i < 4; ++i)
    writers.emplace_back([&, i]{ store.set("key" + std::to_string(i), "value" + std::to_string(i)); });

for (auto& t : readers) t.join();
for (auto& t : writers) t.join();
// every key ever set() is get()-able with its correct value
```

Many reader threads and a few writer threads hit the same `ConfigStore` concurrently; the
20 readers should overlap each other freely, while each `set()` gets exclusive access.
