### How it's called

```cpp
CounterBank<8> bank;

std::vector<std::thread> ts;
for (std::size_t i = 0; i < 8; ++i) {
    ts.emplace_back([&bank, i]{
        for (int n = 0; n < 1'000'000; ++n) bank.increment(i);
    });
}
for (auto& t : ts) t.join();

for (std::size_t i = 0; i < 8; ++i) {
    // bank.get(i) == 1'000'000 for every i
    std::printf("counter %zu at address %#lx\n", i, bank.address_of(i));
}
// every address_of(i) falls in a different 64-byte-aligned block
```

Each of the 8 threads only ever calls `increment` with its own fixed index.
