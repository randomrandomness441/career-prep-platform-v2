// Harness for "Thread Pool with future-returning submit()".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int main() {
    // ── 1. basic correctness: results come back through the future ───────
    {
        ThreadPool pool(4);
        auto f1 = pool.submit([] { return 2 + 2; });
        auto f2 = pool.submit([](int a, int b) { return a * b; }, 6, 7);
        auto f3 = pool.submit([](std::string s) { return s + "!"; }, std::string("hi"));
        if (f1.get() != 4) { std::printf("basic: 2+2 wrong\n"); return 1; }
        if (f2.get() != 42) { std::printf("basic: 6*7 wrong\n"); return 1; }
        if (f3.get() != "hi!") { std::printf("basic: string task wrong\n"); return 1; }
    }

    // ── 2. an exception thrown inside a task comes back through get() ────
    {
        ThreadPool pool(2);
        auto f = pool.submit([]() -> int { throw std::runtime_error("boom"); });
        bool caught = false;
        try {
            f.get();
        } catch (const std::runtime_error& e) {
            caught = (std::string(e.what()) == "boom");
        }
        if (!caught) { std::printf("exception from a task did not propagate through get()\n"); return 1; }

        // the pool itself must still be usable after a task threw
        auto g = pool.submit([] { return 99; });
        if (g.get() != 99) { std::printf("pool unusable after a task threw\n"); return 1; }
    }

    // ── 3. many threads submitting concurrently, many tasks each ─────────
    {
        ThreadPool pool(6);
        const int kSubmitters = 8;
        const int kPerThread = 500;
        std::atomic<long long> sum{0};
        std::vector<std::thread> submitters;
        for (int s = 0; s < kSubmitters; ++s) {
            submitters.emplace_back([&pool, &sum] {
                for (int i = 0; i < kPerThread; ++i) {
                    SHAKE();
                    auto f = pool.submit([](int x) { return x * x; }, i % 17);
                    sum.fetch_add(f.get(), std::memory_order_relaxed);
                }
            });
        }
        for (auto& t : submitters) t.join();

        long long expected_per_thread = 0;
        for (int i = 0; i < kPerThread; ++i) {
            long long v = i % 17;
            expected_per_thread += v * v;
        }
        long long expected = expected_per_thread * kSubmitters;
        if (sum.load() != expected) {
            std::printf("concurrent submitters: sum=%lld expected=%lld\n",
                        sum.load(), expected);
            return 1;
        }
    }

    // ── 4. graceful shutdown: nothing queued is ever dropped ─────────────
    // Two workers are pinned inside a "blocker" task, held there by a gate
    // this thread controls. While both are pinned, kQueued more tasks are
    // submitted -- they are provably still sitting in the queue, because no
    // worker is free to have taken them. The pool is then destroyed while
    // the workers are still blocked; a helper thread releases the gate a
    // little later so the destructor's join() can eventually complete. A
    // pool that drops queued work on shutdown finishes with done == 0. A
    // pool that shuts down gracefully finishes with done == kQueued, always.
    for (int trial = 0; trial < 5; ++trial) {
        auto pool = std::make_unique<ThreadPool>(2);

        std::mutex gate_m;
        std::condition_variable gate_cv;
        bool release_gate = false;
        std::atomic<int> started{0};

        const int kQueued = 40;
        std::atomic<int> done{0};

        for (int i = 0; i < 2; ++i) {
            pool->submit([&] {
                started.fetch_add(1, std::memory_order_relaxed);
                std::unique_lock<std::mutex> lk(gate_m);
                gate_cv.wait(lk, [&] { return release_gate; });
            });
        }
        // Both workers are now committed to picking up a blocker task each
        // (the pool has exactly 2 workers). Wait until both have actually
        // started running theirs, so the kQueued submissions below are
        // guaranteed to land in the queue rather than be picked up early.
        while (started.load(std::memory_order_relaxed) < 2) std::this_thread::yield();

        for (int i = 0; i < kQueued; ++i) {
            pool->submit([&done] { done.fetch_add(1, std::memory_order_relaxed); });
        }

        std::thread releaser([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            {
                std::lock_guard<std::mutex> lk(gate_m);
                release_gate = true;
            }
            gate_cv.notify_all();
        });

        pool.reset();   // ~ThreadPool(): must drain the queue before any worker exits
        releaser.join();

        if (done.load() != kQueued) {
            std::printf("trial %d: %d of %d queued tasks ran before shutdown finished "
                        "(and %d blockers had started) -- queued work was dropped\n",
                        trial, done.load(), kQueued, started.load());
            return 1;
        }
    }

    std::printf("basic results, exception propagation, %d threads submitting "
                "concurrently, and %d trials of graceful shutdown: all correct, "
                "no task ever dropped\n", 8, 5);
    return 0;
}
