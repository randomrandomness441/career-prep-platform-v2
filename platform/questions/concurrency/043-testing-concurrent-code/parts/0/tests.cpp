// Harness for a hand-rolled Spinlock. Includes the candidate's file
// verbatim. This question's reading is about testing methodology, so this
// harness deliberately checks the SAME bug three different ways, mirroring
// the three signals this course's own runner uses: a direct mutual-
// exclusion violation count, a lost-update count on a plain shared counter,
// and (via the platform's own TSan/stress stages, not this file) a data
// race and a many-repeats-only failure.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    constexpr int kThreads = 8;
    constexpr int kIncrements = 2000;

    Spinlock lock;
    long shared_counter = 0;           // NOT atomic on purpose: only safe if
                                        // the lock genuinely gives exclusion
    std::atomic<int> holders{0};       // how many threads are inside the
                                        // critical section RIGHT NOW
    std::atomic<int> violations{0};    // > 1 holder at once, observed directly

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < kIncrements; ++i) {
                lock.lock();
                int now = holders.fetch_add(1, std::memory_order_relaxed) + 1;
                if (now > 1) violations.fetch_add(1, std::memory_order_relaxed);
                ++shared_counter;
                SHAKE();
                holders.fetch_sub(1, std::memory_order_relaxed);
                lock.unlock();
            }
        });
    }
    for (auto& t : threads) t.join();

    if (violations.load() > 0) {
        std::printf("mutual exclusion violated %d times: more than one thread was inside "
                    "the critical section at once\n", violations.load());
        return 1;
    }
    long expected = static_cast<long>(kThreads) * kIncrements;
    if (shared_counter != expected) {
        std::printf("shared_counter = %ld, expected %ld -- lost updates on a plain "
                    "(non-atomic) counter mean the lock did not actually exclude\n",
                    shared_counter, expected);
        return 1;
    }

    std::printf("%d threads x %d increments: zero mutual-exclusion violations, "
                "shared_counter exact\n", kThreads, kIncrements);
    return 0;
}
