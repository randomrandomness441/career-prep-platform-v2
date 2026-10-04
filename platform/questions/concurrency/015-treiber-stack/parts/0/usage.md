### How it's called

```cpp
LockFreeStack<int> s;

std::vector<std::thread> producers, consumers;
for (int i = 0; i < 4; ++i)
    producers.emplace_back([&s, i]{ for (int n = 0; n < 1000; ++n) s.push(i * 1000 + n); });
for (int i = 0; i < 4; ++i)
    consumers.emplace_back([&s]{
        int v;
        while (still_expecting_values) {
            if (s.pop(v)) record(v);   // false just means "nothing there right now, try again"
        }
    });
for (auto& t : producers) t.join();
for (auto& t : consumers) t.join();
// every value pushed by the 4 producers was recorded by consumers exactly once
```

Four producers and four consumers hit the same stack at once, no coordination between
them beyond what the stack itself provides.
