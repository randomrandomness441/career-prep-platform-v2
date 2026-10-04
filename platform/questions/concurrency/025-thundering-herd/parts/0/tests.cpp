// Harness for SlotPool (Thundering Herd). Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    // ── Part A: basic correctness. A pool of 3 slots, 24 threads each
    // acquiring and releasing 40 times. Never more than 3 in use at once,
    // and every acquire/release pairs up cleanly.
    {
        constexpr int kSlots = 3;
        constexpr int kThreads = 24;
        constexpr int kRounds = 40;

        SlotPool pool(kSlots);
        std::atomic<int> in_use{0};
        std::atomic<int> peak{0};
        std::atomic<int> total_acquires{0};

        std::vector<std::thread> threads;
        for (int i = 0; i < kThreads; ++i) {
            threads.emplace_back([&] {
                for (int r = 0; r < kRounds; ++r) {
                    pool.acquire();
                    int now = in_use.fetch_add(1, std::memory_order_relaxed) + 1;
                    int seen = peak.load(std::memory_order_relaxed);
                    while (now > seen &&
                           !peak.compare_exchange_weak(seen, now, std::memory_order_relaxed)) {}
                    if (now > kSlots) {
                        std::printf("%d threads held a slot at once, pool has %d\n", now, kSlots);
                        std::exit(1);
                    }
                    SHAKE();
                    in_use.fetch_sub(1, std::memory_order_relaxed);
                    total_acquires.fetch_add(1, std::memory_order_relaxed);
                    pool.release();
                }
            });
        }
        for (auto& t : threads) t.join();

        if (total_acquires.load() != kThreads * kRounds) {
            std::printf("%d total acquires, expected %d\n",
                        total_acquires.load(), kThreads * kRounds);
            return 1;
        }
        if (peak.load() < 1 || peak.load() > kSlots) {
            std::printf("peak concurrent holders was %d, expected 1..%d\n", peak.load(), kSlots);
            return 1;
        }
    }

    // ── Part B: the thundering herd itself. One slot, N waiters all parked
    // at once, then the slot is released and re-acquired N times in strict
    // sequence -- so at release k, exactly (N - k + 1) threads are asleep.
    // A notify_all design wakes all of them on every release; a notify_one
    // design wakes exactly the one that can proceed. wasted_wakeups counts
    // "woke up, rechecked, found nothing" -- deterministic, not a timing
    // measurement: notify_one always produces 0 here, notify_all always
    // produces hundreds, on every run.
    {
        constexpr int kWaiters = 100;
        constexpr long kWastedBudget = 30;  // correct: ~0. naive: ~N(N-1)/2 = 4950.

        SlotPool pool(0);
        std::atomic<int> acquired{0};

        std::vector<std::thread> threads;
        for (int i = 0; i < kWaiters; ++i)
            threads.emplace_back([&] {
                pool.acquire();
                acquired.fetch_add(1, std::memory_order_relaxed);
            });

        // Wait for every thread to have entered acquire() and parked on the
        // condition variable before releasing anything -- otherwise a fast
        // thread could grab a slot before it has any competition, and the
        // herd never forms.
        while (pool.started.load(std::memory_order_relaxed) < kWaiters) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        for (int k = 1; k <= kWaiters; ++k) {
            pool.release();
            while (acquired.load(std::memory_order_relaxed) < k) std::this_thread::yield();
        }
        for (auto& t : threads) t.join();

        long wasted = pool.wasted_wakeups.load(std::memory_order_relaxed);
        if (wasted > kWastedBudget) {
            std::printf("%ld wasted wakeups releasing %d slots one at a time to %d waiters "
                        "(budget: %ld). Every waiter shares the same predicate, so only the "
                        "thread that can actually proceed needs to be woken -- this looks "
                        "like notify_all where notify_one would do.\n",
                        wasted, kWaiters, kWaiters, kWastedBudget);
            return 1;
        }
    }

    std::printf("bounded pool respects capacity, and releasing one slot at a time to "
                "100 waiters wastes no wakeups\n");
    return 0;
}
