### How it's called

```cpp
backpressure_queue<int> q(4, overflow_policy::DropOldest);

std::thread producer([&q]{
    for (int i = 0; i < 1000; ++i) q.push(i);   // fast producer
});
std::thread consumer([&q]{
    while (std::optional<int> v = q.pop()) { /* slow: process(*v) */ }
});
producer.join();
q.close();
consumer.join();

std::printf("dropped %zu items\n", q.dropped());
```

With `overflow_policy::Block` instead, a full queue makes `push` wait (rather than drop)
until `pop()` or `close()` makes room.
