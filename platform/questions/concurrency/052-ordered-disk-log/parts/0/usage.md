### How it's called

```cpp
OrderedLog log;

std::vector<std::thread> ts;
for (int i = 0; i < 8; ++i) {
    ts.emplace_back([&log, i]{
        for (int n = 0; n < 100; ++n)
            log.write_record("thread " + std::to_string(i) + " record " + std::to_string(n));
    });
}
for (auto& t : ts) t.join();

std::vector<std::string> all = log.contents();
// all.size() == 800; all[k] is exactly the record that was assigned ticket k,
// regardless of which thread's write physically landed first
```
