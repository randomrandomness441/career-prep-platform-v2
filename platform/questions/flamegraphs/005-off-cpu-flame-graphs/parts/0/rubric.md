A good answer covers:

- **Why the CPU graph looks unchanged.** A CPU flame graph only samples threads actively
  running on-core. Time spent blocked — waiting on a lock, a disk read, a network call,
  a sleep — costs zero CPU and is invisible to it by construction, no matter how long the
  wait lasts. A latency regression caused by more waiting, not more computing, produces
  no visible change in a CPU flame graph at all. The answer should name this mechanism,
  not just assert "CPU graphs miss some things."
- **Which graph shows the contended mutex, and what the top frame looks like.** The
  off-CPU flame graph would show it — a wide box for threads that are off-CPU. The top
  frame would be whatever lock/wait primitive the blocked thread is sitting inside (e.g.
  a futex wait, a mutex lock call) — a blocked thread is still "on the stack" inside that
  wait call even though it isn't executing; off-CPU sampling captures how long it stayed
  there, not what it computed while there (because it computed nothing).
- **Longer CPU capture doesn't fix it.** No — the blind spot isn't a duration problem,
  it's a category problem. A CPU flame graph fundamentally never samples off-CPU time no
  matter how long you run it; running longer gives you more samples of the same kind of
  (on-CPU) data, not a new kind of data. The fix is switching tools (off-CPU profiling),
  not extending the same one.

NEEDS_WORK if the answer suggests a longer or more frequent CPU capture would surface
blocked-thread time, or can't name what kind of cost is invisible to on-CPU sampling.

## Code

**The regression — a mutex that's become heavily contended, invisible to a CPU profile:**
```cpp
void process(Order& o) {
    std::lock_guard<std::mutex> lk(shared_mutex);   // threads now queue here, blocked
    apply(o);
}
```

**Wrong diagnostic move — reaching for another (or longer) CPU capture:**
```
perf record -F 999 -a -g -- sleep 60     # still on-CPU only; contention stays invisible
```

**Correct — capture off-CPU time, which shows exactly where threads are blocked:**
```
perf record -e sched:sched_switch -a -g -- sleep 30
perf script | stackcollapse-perf.pl --kernel | flamegraph.pl --color=io > offcpu.svg
```

