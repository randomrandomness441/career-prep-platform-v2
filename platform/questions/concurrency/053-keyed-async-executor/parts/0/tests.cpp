// Harness for "Keyed Async Executor". Includes the candidate's file
// verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

int main() {
    constexpr int kKeys = 12;
    constexpr int kTasksPerKey = 200;
    constexpr int kWorkers = 8;

    std::vector<std::atomic<bool>> in_progress(kKeys);
    for (auto& f : in_progress) f.store(false);
    std::vector<std::atomic<int>> next_expected(kKeys);
    for (auto& n : next_expected) n.store(0);
    std::atomic<int> order_violations{0};
    std::atomic<int> overlap_violations{0};
    std::atomic<int> completed{0};

    {
        KeyedAsyncExecutor exec(kWorkers);
        std::vector<std::thread> submitters;
        for (int k = 0; k < kKeys; ++k) {
            submitters.emplace_back([&, k] {
                for (int seq = 0; seq < kTasksPerKey; ++seq) {
                    exec.submit(std::to_string(k), [&, k, seq] {
                        if (in_progress[static_cast<std::size_t>(k)].exchange(true)) {
                            overlap_violations.fetch_add(1, std::memory_order_relaxed);
                        }
                        int expected = next_expected[static_cast<std::size_t>(k)].load();
                        if (expected != seq) order_violations.fetch_add(1, std::memory_order_relaxed);
                        next_expected[static_cast<std::size_t>(k)].store(seq + 1);
                        SHAKE();
                        in_progress[static_cast<std::size_t>(k)].store(false);
                        completed.fetch_add(1, std::memory_order_relaxed);
                    });
                    if ((seq & 15) == 0) SHAKE();
                }
            });
        }
        for (auto& t : submitters) t.join();
        // exec goes out of scope here -- its destructor must drain
        // everything already submitted before returning.
    }

    if (overlap_violations.load() != 0) {
        std::printf("%d times, two tasks for the SAME key ran overlapping -- per-key "
                    "mutual exclusion was violated\n", overlap_violations.load());
        return 1;
    }
    if (order_violations.load() != 0) {
        std::printf("%d times, a task ran out of submission order relative to its own "
                    "key's other tasks\n", order_violations.load());
        return 1;
    }
    int expected_total = kKeys * kTasksPerKey;
    if (completed.load() != expected_total) {
        std::printf("%d of %d submitted tasks completed before the executor finished "
                    "shutting down\n", completed.load(), expected_total);
        return 1;
    }

    std::printf("%d keys x %d tasks each: zero same-key overlaps, zero ordering "
                "violations, all tasks completed\n", kKeys, kTasksPerKey);
    return 0;
}
