`++counter_` on a plain `long` is a read, an add, and a write — three separate steps. Two
threads can each do the read before either does the write, and one increment gets lost.
That's the bug in the naive version; it's a genuine data race, not just a style problem.
---
A `std::mutex` around the three steps fixes it. But look at requirement 3 again: there's
only one variable, one operation, no second thing it needs to stay consistent with. That's
exactly the case `std::atomic` was built for — the hardware can do "read, add, write" as one
indivisible instruction, with no lock, no OS involvement, no thread ever sleeping.
---
```cpp
class HotCounter {
    std::atomic<long> counter_{0};
public:
    void increment() noexcept { counter_.fetch_add(1, std::memory_order_relaxed); }
    long get() const noexcept { return counter_.load(std::memory_order_relaxed); }
};
```
`relaxed` is enough here: nothing else needs to be ordered around this counter, so buy no
more than requirement 3 asks for. A mutex would also be correct — just slower for exactly
this shape of problem, which the reading measures for real.
