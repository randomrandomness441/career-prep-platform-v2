### How it's called

```cpp
SpscRing<int, 1024> r;   // one producer, one consumer -- never more of either

std::thread producer([&r]{
    for (int i = 0; i < 20000; ++i)
        while (!r.push(i)) { /* ring full, retry */ }
});
std::thread consumer([&r]{
    int v;
    for (int i = 0; i < 20000; ++i) {
        while (!r.pop(v)) { /* ring empty, retry */ }
        // v must equal i, in order
    }
});
producer.join();
consumer.join();
```

Exactly one thread ever calls `push`; exactly one (a different one) ever calls `pop`. Both
retry in a spin loop on `false` rather than blocking.
