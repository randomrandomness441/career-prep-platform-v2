### How it's called

```cpp
std::vector<long long> totals = run_phased_simulation(
    /* n_workers */ 8,
    /* n_phases  */ 5,
    /* compute      */ [](int worker, int phase) -> long long {
        return worker + phase;          // this worker's contribution to this phase
    },
    /* on_phase_end  */ [](int phase, long long total) {
        std::cout << "phase " << phase << " total: " << total << '\n';
    });
// totals.size() == 5, totals[p] == sum of compute(w, p) over all 8 workers

run_phased_simulation(0, 5, compute, on_phase_end);   // returns {} immediately
```

You spawn the 8 worker threads yourself inside the function; the caller only ever sees
one call in, one vector out.
