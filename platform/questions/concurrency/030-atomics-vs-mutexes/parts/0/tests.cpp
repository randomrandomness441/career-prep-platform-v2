// Harness for HotCounter (Atomics vs Mutexes). Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    // 8 threads hammering one shared counter -- enough real contention that
    // a plain `++counter_` loses updates on essentially every run (measured:
    // ~80% of increments lost, consistently, across repeated trials). A
    // correct implementation (mutex or atomic) gets the exact total every
    // single time -- this is a real correctness bug, not a timing gamble.
    //
    // kIters is `volatile` on purpose: with a compile-time-constant trip
    // count, an optimizing compiler is *allowed* to hoist the whole racy
    // loop into a single "load, add N, store" at the end of each thread
    // (the data race is undefined behaviour, so the compiler may assume no
    // other thread touches counter_ in between) -- and it does exactly that
    // at -O1 here, which collapses 200000 real read-modify-writes per
    // thread down to one, making the bug vanish almost every run. volatile
    // forces a genuine memory read of the loop bound every iteration, which
    // is enough to defeat that optimization and restore the real interleaving.
    constexpr int kThreads = 8;
    volatile long kIters = 200000;

    HotCounter counter;
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            for (long k = 0; k < kIters; ++k) {
                counter.increment();
            }
        });
    }
    for (auto& t : threads) t.join();

    long expected = static_cast<long>(kThreads) * kIters;
    long got = counter.get();
    if (got != expected) {
        std::printf("%ld threads x %ld increments: got %ld, expected %ld (%ld lost)\n",
                    (long)kThreads, kIters, got, expected, expected - got);
        return 1;
    }

    std::printf("%d threads x %ld increments each: exact total, no updates lost\n",
                kThreads, kIters);
    return 0;
}
