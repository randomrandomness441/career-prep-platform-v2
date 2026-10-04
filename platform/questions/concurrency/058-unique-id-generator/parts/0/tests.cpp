// Harness for UniqueIdGenerator. Includes the candidate's class verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

int main() {
    // 1. Basic packing: sequence increments within a tick, resets on tick change.
    {
        std::atomic<std::uint64_t> clk{1000};
        UniqueIdGenerator g(7, [&] { return clk.load(); });

        std::uint64_t a = g.next_id();
        std::uint64_t b = g.next_id();
        if (a == b) { std::printf("two sequential calls returned the same id\n"); return 1; }
        if (b <= a) {
            std::printf("sequence must increase within the same tick (a=%llu b=%llu)\n",
                        (unsigned long long)a, (unsigned long long)b);
            return 1;
        }
        clk.store(1001);
        std::uint64_t c = g.next_id();
        if (c <= b) {
            std::printf("id after a tick change must be greater than before it\n");
            return 1;
        }
    }

    // 2. Two different generators, same clock, never collide -- even interleaved.
    {
        std::atomic<std::uint64_t> clk{5000};
        UniqueIdGenerator g1(1, [&] { return clk.load(); });
        UniqueIdGenerator g2(2, [&] { return clk.load(); });
        std::set<std::uint64_t> seen;
        for (int i = 0; i < 100; ++i) {
            if (!seen.insert(g1.next_id()).second || !seen.insert(g2.next_id()).second) {
                std::printf("two generators with different worker_id collided\n");
                return 1;
            }
        }
    }

    // 3. The reset race: many threads pile into the SAME tick transition at once.
    for (int trial = 0; trial < 15; ++trial) {
        std::atomic<std::uint64_t> clk{1};
        UniqueIdGenerator g(3, [&] { return clk.load(); });
        (void)g.next_id();   // prime: last tick becomes 1

        clk.store(2);   // the tick every thread below will race into

        constexpr int kThreads = 16;
        std::atomic<bool> go{false};
        std::vector<std::uint64_t> ids(kThreads);
        std::vector<std::thread> ts;
        for (int i = 0; i < kThreads; ++i) {
            ts.emplace_back([&, i] {
                while (!go.load()) { /* spin until released together */ }
                ids[static_cast<std::size_t>(i)] = g.next_id();
            });
        }
        go.store(true);
        for (auto& t : ts) t.join();

        std::set<std::uint64_t> distinct(ids.begin(), ids.end());
        if (distinct.size() != ids.size()) {
            std::printf("trial %d: %zu threads racing one tick transition produced only "
                        "%zu distinct ids -- duplicates from the reset race\n",
                        trial, ids.size(), distinct.size());
            return 1;
        }
    }

    // 4. Sequence overflow within one tick must roll the tick forward, not wrap.
    {
        std::atomic<std::uint64_t> clk{100};
        UniqueIdGenerator g(9, [&] { return clk.load(); });
        std::set<std::uint64_t> seen;
        bool advanced_past_clock = false;
        for (int i = 0; i < 5000; ++i) {   // more than the 4096 sequence numbers available
            std::uint64_t id = g.next_id();
            if (!seen.insert(id).second) {
                std::printf("call %d: sequence overflow produced a duplicate id -- the "
                            "generator must advance its own tick, not wrap the sequence\n", i);
                return 1;
            }
            if ((id >> 22) > 100) advanced_past_clock = true;
        }
        if (!advanced_past_clock) {
            std::printf("5000 ids in one tick with only 4096 sequence numbers, but the "
                        "generator's tick never advanced past the injected clock\n");
            return 1;
        }
    }

    // 5. Heavy concurrent stress with a real advancing clock.
    for (int trial = 0; trial < 5; ++trial) {
        std::atomic<std::uint64_t> clk{0};
        std::atomic<bool> tick_thread_stop{false};
        std::thread ticker([&] {
            while (!tick_thread_stop.load()) {
                SHAKE();
                clk.fetch_add(1);
            }
        });

        UniqueIdGenerator g(4, [&] { return clk.load(); });
        constexpr int kThreads = 6, kPerThread = 500;
        std::vector<std::vector<std::uint64_t>> results(kThreads);
        std::vector<std::thread> ts;
        for (int t = 0; t < kThreads; ++t) {
            results[static_cast<std::size_t>(t)].reserve(kPerThread);
            ts.emplace_back([&, t] {
                for (int i = 0; i < kPerThread; ++i)
                    results[static_cast<std::size_t>(t)].push_back(g.next_id());
            });
        }
        for (auto& t : ts) t.join();
        tick_thread_stop.store(true);
        ticker.join();

        std::set<std::uint64_t> all;
        for (auto& v : results)
            for (auto id : v)
                if (!all.insert(id).second) {
                    std::printf("trial %d: duplicate id under heavy concurrent load\n", trial);
                    return 1;
                }
        if (all.size() != static_cast<std::size_t>(kThreads * kPerThread)) {
            std::printf("trial %d: expected %d unique ids, got %zu\n",
                        trial, kThreads * kPerThread, all.size());
            return 1;
        }
    }

    std::printf("packing, cross-generator uniqueness, the reset race, sequence overflow "
                "and heavy concurrent load all correct\n");
    return 0;
}
