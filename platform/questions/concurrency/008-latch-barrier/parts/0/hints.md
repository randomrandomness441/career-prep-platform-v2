The whole synchronisation is three lines. `std::barrier` is constructed with the number of
participants and, optionally, a function to run between phases:

```cpp
std::barrier sync(n_workers, [&]() noexcept { /* runs once, between phases */ });
...
sync.arrive_and_wait();   // in each worker, at the end of each phase
```

Each worker's loop is: compute my part, `arrive_and_wait()`, repeat. Nothing else. The
barrier resets itself for the next phase, which is exactly what your hand-rolled counter
could not do safely.
---
The completion function is where the aggregation belongs, and it is worth being precise
about why: `std::barrier` runs it **after the last arrival and before it releases anybody**.
So at the instant it runs, every worker is provably parked. It needs no lock, and nothing
it reads can be changing under it.

Two consequences:
- Do not aggregate in a worker after `arrive_and_wait()` returns. By then the other workers
  have been released and are already writing next phase's values.
- It must not throw, and it runs on *whichever* thread happened to arrive last, which
  changes from phase to phase. Do not put thread-specific state in it.
---
```cpp
std::vector<std::atomic<long long>> slots(n_workers);
std::vector<long long> totals;

auto aggregate = [&]() noexcept {
    long long total = 0;
    for (auto& s : slots) total += s.load();
    int p = static_cast<int>(totals.size());   // phase number, for free
    totals.push_back(total);
    if (on_phase_end) on_phase_end(p, total);
};

std::barrier sync(n_workers, aggregate);

// worker w:
for (int p = 0; p < n_phases; ++p) {
    slots[w].store(compute(w, p));
    sync.arrive_and_wait();
}
```

`totals` needs no protection even though a different thread runs the completion each phase:
each completion is separated from the next by a full phase of the barrier's own ordering, so
they are never concurrent. That is also why `totals.size()` is a valid phase counter — it is
only ever read and written from inside the completion.
