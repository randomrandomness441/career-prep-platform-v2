// Harness for "Lock-Free Hash Map". Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    // ── 1. heavy contention: many threads racing to insert the SAME small
    // set of keys at once. Exactly one insert() per key may return true.
    {
        constexpr int kKeys = 10;
        constexpr int kThreadsPerKey = 40;

        LockFreeHashMap<int, int> map(8);
        std::vector<std::atomic<int>> wins(kKeys);
        for (auto& w : wins) w.store(0, std::memory_order_relaxed);

        std::vector<std::thread> threads;
        for (int k = 0; k < kKeys; ++k) {
            for (int t = 0; t < kThreadsPerKey; ++t) {
                threads.emplace_back([&, k, t] {
                    SHAKE();
                    if (map.insert(k, k * 100 + t)) {
                        wins[static_cast<std::size_t>(k)].fetch_add(1, std::memory_order_relaxed);
                    }
                });
            }
        }
        for (auto& th : threads) th.join();

        for (int k = 0; k < kKeys; ++k) {
            int w = wins[static_cast<std::size_t>(k)].load();
            if (w != 1) {
                std::printf("key %d: insert() returned true %d times, expected exactly 1 "
                            "(duplicate node in the bucket if > 1)\n", k, w);
                return 1;
            }
        }
        for (int k = 0; k < kKeys; ++k) {
            int v = -1;
            if (!map.find(k, &v)) {
                std::printf("key %d: find() failed after a successful insert\n", k);
                return 1;
            }
        }
    }

    // ── 2. many distinct keys, no collisions, ordinary correctness ────────
    {
        constexpr int kKeys = 5000;
        LockFreeHashMap<int, int> map(64);
        std::vector<std::thread> threads;
        for (int t = 0; t < 8; ++t) {
            threads.emplace_back([&, t] {
                for (int k = t; k < kKeys; k += 8) {
                    if (!map.insert(k, k * 2)) {
                        std::printf("key %d: insert() unexpectedly returned false (no "
                                    "other thread should have touched this key)\n", k);
                        std::exit(1);
                    }
                }
            });
        }
        for (auto& t : threads) t.join();

        for (int k = 0; k < kKeys; ++k) {
            int v = -1;
            if (!map.find(k, &v) || v != k * 2) {
                std::printf("key %d: find() returned %s%d, expected %d\n",
                            k, map.find(k, &v) ? "" : "(missing) ", v, k * 2);
                return 1;
            }
        }
        if (map.find(999999, nullptr)) {
            std::printf("find() reported a key that was never inserted\n");
            return 1;
        }
    }

    std::printf("heavy-contention duplicate-key check and 5000 distinct concurrent "
                "inserts: every key correct, no duplicates\n");
    return 0;
}
