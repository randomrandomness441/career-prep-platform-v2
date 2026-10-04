// Harness for "Async Gather". Includes the candidate's file verbatim.
#include "solution.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <future>
#include <stdexcept>
#include <vector>

int main() {
    // ── 1. results must come back in INPUT order, even when tasks finish
    // in the opposite order. Each task does one atomic operation (a
    // fetch_add on a shared counter -- standing in for "async tasks
    // composed of atomic operations") and then sleeps for a duration that
    // decreases with index, so task N-1 finishes first and task 0 finishes
    // last.
    {
        constexpr int kTasks = 20;
        std::atomic<int> ops_performed{0};

        std::vector<std::future<int>> futures;
        for (int i = 0; i < kTasks; ++i) {
            futures.push_back(std::async(std::launch::async, [i, &ops_performed] {
                ops_performed.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::sleep_for(std::chrono::milliseconds(kTasks - i));
                return i;
            }));
        }

        std::vector<int> results = gather(std::move(futures));

        if (results.size() != static_cast<std::size_t>(kTasks)) {
            std::printf("gather() returned %zu results, expected %d\n", results.size(), kTasks);
            return 1;
        }
        for (int i = 0; i < kTasks; ++i) {
            if (results[static_cast<std::size_t>(i)] != i) {
                std::printf("results[%d] = %d, expected %d -- gather() returned results "
                            "in completion order, not input order\n",
                            i, results[static_cast<std::size_t>(i)], i);
                return 1;
            }
        }
        if (ops_performed.load() != kTasks) {
            std::printf("%d atomic operations performed, expected %d\n",
                        ops_performed.load(), kTasks);
            return 1;
        }
    }

    // ── 2. an exception from any task propagates out of gather() ─────────
    {
        std::vector<std::future<int>> futures;
        futures.push_back(std::async(std::launch::async, [] { return 1; }));
        futures.push_back(std::async(std::launch::async, []() -> int {
            throw std::runtime_error("task failed");
        }));
        futures.push_back(std::async(std::launch::async, [] { return 3; }));

        bool caught = false;
        try {
            gather(std::move(futures));
        } catch (const std::runtime_error& e) {
            caught = true;
        }
        if (!caught) {
            std::printf("a task's exception did not propagate out of gather()\n");
            return 1;
        }
    }

    std::printf("20 tasks finishing in reverse order still gathered in input order, and "
                "a task's exception propagates correctly\n");
    return 0;
}
