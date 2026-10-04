### How it's called

```cpp
std::atomic<std::uint64_t> fake_now{1000};   // an injected, test-controllable clock
UniqueIdGenerator gen(/*worker_id=*/7, [&]{ return fake_now.load(); });

std::uint64_t a = gen.next_id();
std::uint64_t b = gen.next_id();
// a != b; both encode worker_id 7 and tick 1000, with sequence 0 then 1

fake_now.store(1001);
std::uint64_t c = gen.next_id();
// c's tick is 1001, sequence resets to 0

// two DIFFERENT generators never collide, even with the same clock and heavy concurrent load:
UniqueIdGenerator gen_a(1, [&]{ return fake_now.load(); });
UniqueIdGenerator gen_b(2, [&]{ return fake_now.load(); });

std::vector<std::thread> ts;
std::mutex om; std::set<std::uint64_t> seen;
for (int i = 0; i < 8; ++i) {
    ts.emplace_back([&, i]{
        auto& g = (i % 2 == 0) ? gen_a : gen_b;
        for (int n = 0; n < 10000; ++n) {
            std::uint64_t id = g.next_id();
            std::lock_guard<std::mutex> lk(om);
            seen.insert(id);   // size at the end must equal total calls made
        }
    });
}
for (auto& t : ts) t.join();
```

In production, `clock` would be `[]{ return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count(); }` — the injection point exists purely so tests can control tick boundaries precisely.
