### How it's called

```cpp
Spinlock lk;
long shared_counter = 0;

std::vector<std::thread> ts;
for (int i = 0; i < 8; ++i) {
    ts.emplace_back([&]{
        for (int n = 0; n < 2000; ++n) {
            lk.lock();
            ++shared_counter;        // only one thread ever inside here at once
            lk.unlock();
        }
    });
}
for (auto& t : ts) t.join();
// shared_counter == 16000, exactly, every run
```
