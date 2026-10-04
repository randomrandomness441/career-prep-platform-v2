### How it's called

```cpp
bounded_queue q(2);   // shelf holds 2 plates

std::vector<std::thread> cooks, waiters;
for (int i = 0; i < 4; ++i)
    cooks.emplace_back([&q, i]{ for (int n = 0; n < 100; ++n) q.enqueue(i * 100 + n); });
for (int i = 0; i < 4; ++i)
    waiters.emplace_back([&q]{
        while (std::optional<int> v = q.dequeue()) { /* use *v */ }
        // dequeue() returns nullopt once the queue is closed AND drained
    });

for (auto& t : cooks) t.join();
q.close();                                   // wakes every waiting thread
for (auto& t : waiters) t.join();
```

Four cooks and four waiters run at once against one shared queue -- that mix, at a small
capacity, is exactly the case that deadlocks a naive implementation.
