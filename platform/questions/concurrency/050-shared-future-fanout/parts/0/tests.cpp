// Harness for "Fan-Out with shared_future".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {
constexpr long long kAnswer = 123456789LL;
}

int main() {
    // ── 1. sequential re-reads, ONE thread, no concurrency at all ────────
    // This is the fact the question is actually testing: std::future::get()
    // works once. A type meant for fan-out has to survive being read more
    // than once before it is ever asked to survive concurrent readers.
    {
        std::atomic<int> calls{0};
        SharedComputation<long long> sc([&] {
            calls.fetch_add(1, std::memory_order_relaxed);
            return kAnswer;
        });
        for (int i = 0; i < 5; ++i) {
            long long v = sc.get();
            if (v != kAnswer) {
                std::printf("sequential get() #%d returned %lld, expected %lld\n",
                            i + 1, v, kAnswer);
                return 1;
            }
        }
        if (calls.load() != 1) {
            std::printf("work() ran %d times for 5 sequential get() calls; "
                        "a fan-out result must be computed exactly once\n",
                        calls.load());
            return 1;
        }
    }

    // ── 2. fan-out: many threads all need the one result, all at once ────
    const int kTrials = 20;
    const int kReaders = 12;
    for (int trial = 0; trial < kTrials; ++trial) {
        std::atomic<int> calls{0};
        const long long expected = kAnswer + trial;
        SharedComputation<long long> sc([&calls, expected] {
            calls.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            return expected;
        });

        std::atomic<int> ok{0};
        std::vector<std::thread> readers;
        readers.reserve(kReaders);
        for (int i = 0; i < kReaders; ++i) {
            readers.emplace_back([&] {
                SHAKE();
                long long v = sc.get();
                if (v == expected) ok.fetch_add(1, std::memory_order_relaxed);
            });
        }
        for (auto& t : readers) t.join();

        if (ok.load() != kReaders) {
            std::printf("trial %d: only %d of %d concurrent readers got the "
                        "correct value\n", trial, ok.load(), kReaders);
            return 1;
        }
        if (calls.load() != 1) {
            std::printf("trial %d: work() ran %d times across %d concurrent "
                        "readers\n", trial, calls.load(), kReaders);
            return 1;
        }
    }

    // ── 3. a struct payload, not just an integer ──────────────────────────
    {
        struct Payload { long long a; long long b; };
        SharedComputation<Payload> sc([] { return Payload{7, 9}; });
        std::atomic<int> ok{0};
        std::vector<std::thread> readers;
        for (int i = 0; i < 8; ++i) {
            readers.emplace_back([&] {
                Payload p = sc.get();
                if (p.a == 7 && p.b == 9) ok.fetch_add(1, std::memory_order_relaxed);
            });
        }
        for (auto& t : readers) t.join();
        if (ok.load() != 8) {
            std::printf("struct payload: only %d of 8 readers got the correct value\n",
                        ok.load());
            return 1;
        }
    }

    std::printf("5 sequential reads, %d trials of %d concurrent readers, and a "
                "struct payload: every reader saw the correct value, computed once\n",
                kTrials, kReaders);
    return 0;
}
