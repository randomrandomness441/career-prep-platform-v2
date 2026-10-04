### How it's called

```cpp
MemoryPool<64, 200> pool;   // 200 blocks of 64 bytes each, allocated once

std::vector<std::thread> ts;
for (int i = 0; i < 8; ++i) {
    ts.emplace_back([&pool]{
        for (int n = 0; n < 10000; ++n) {
            void* p = pool.allocate();
            if (!p) continue;              // pool full right now -- fail fast, try later
            use(p);
            bool ok = pool.deallocate(p);  // true: was a live allocation from this pool
        }
    });
}
for (auto& t : ts) t.join();
// pool.blocks_in_use() == 0 here -- everything borrowed was returned
```

Many threads call `allocate`/`deallocate` concurrently, constantly recycling the same 200
underlying blocks.
