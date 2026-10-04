// Harness for "Read-Write Locks (std::shared_mutex)". Includes the
// candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

int main() {
    // ── concurrent writers on DISTINCT keys, concurrent readers throughout ──
    // std::unordered_map is not safe for concurrent modification even to
    // different keys -- an insert can trigger a rehash that touches the
    // whole table. Two set() calls racing (both holding only a shared lock)
    // are two threads mutating that table with no exclusion between them.
    constexpr int kWriters = 8;
    constexpr int kReaders = 4;
    constexpr int kKeysPerWriter = 2000;

    ConfigStore store;
    std::vector<std::thread> threads;

    for (int w = 0; w < kWriters; ++w) {
        threads.emplace_back([&, w] {
            for (int i = 0; i < kKeysPerWriter; ++i) {
                std::string key = "w" + std::to_string(w) + "_" + std::to_string(i);
                store.set(key, key + "_value");
                SHAKE();
            }
        });
    }
    for (int r = 0; r < kReaders; ++r) {
        threads.emplace_back([&] {
            for (int i = 0; i < kKeysPerWriter; ++i) {
                store.get("w0_0");
                SHAKE();
            }
        });
    }
    for (auto& t : threads) t.join();

    // Every key every writer wrote must be present with the right value.
    // (A crash or a TSan report during the run above is the primary signal
    // this question is testing for; this is the secondary correctness check
    // in case the race happened not to corrupt anything on this run.)
    for (int w = 0; w < kWriters; ++w) {
        for (int i = 0; i < kKeysPerWriter; ++i) {
            std::string key = "w" + std::to_string(w) + "_" + std::to_string(i);
            std::string got = store.get(key);
            if (got != key + "_value") {
                std::printf("key %s: got %s, expected %s\n",
                            key.c_str(), got.c_str(), (key + "_value").c_str());
                return 1;
            }
        }
    }

    std::printf("%d writers x %d keys, %d concurrent readers: every key intact, "
                "no corruption\n", kWriters, kKeysPerWriter, kReaders);
    return 0;
}
