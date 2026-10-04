// Harness for "Stream Counter". Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    constexpr int kThreads = 16;
    // volatile on purpose: with a compile-time-constant trip count, the
    // optimizer can hoist a naive `++count_` loop into one local
    // accumulation plus a single store at the end of the loop, per thread
    // -- which narrows the race window down to one write instead of
    // thousands, making the bug nearly vanish. volatile forces the
    // compiler to treat the bound as unknowable at compile time.
    volatile long kPerThread = 200000;

    StreamCounter counter(8);
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            for (long i = 0; i < kPerThread; ++i) {
                counter.increment();
                if ((i & 4095) == 0) SHAKE();
            }
        });
    }
    for (auto& th : threads) th.join();

    long expected = static_cast<long>(kThreads) * kPerThread;
    long got = counter.total();
    if (got != expected) {
        std::printf("total() = %ld, expected %ld -- %ld increments were lost under "
                    "concurrent load\n", got, expected, expected - got);
        return 1;
    }

    std::printf("%d threads x %ld increments each: total() exact\n", kThreads, kPerThread);
    return 0;
}
