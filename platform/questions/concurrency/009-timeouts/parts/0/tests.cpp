// Harness for "Waiting With a Deadline".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

using clk = std::chrono::steady_clock;
using ms  = std::chrono::milliseconds;

long long since(clk::time_point start) {
    return std::chrono::duration_cast<ms>(clk::now() - start).count();
}

// Every duration here is tens of milliseconds so the whole binary finishes fast.
const long long kTimeout   = 50;    // what we ask wait_for_result for
const long long kBound     = 150;   // generous ceiling on what it may actually take
const long long kNudgeStop = 400;   // how long the nudger keeps disturbing us

}  // namespace

int main() {
    // ── 1. a value that is already there costs no waiting ─────────────────
    {
        result_slot s;
        s.set(42);
        const clk::time_point t0 = clk::now();
        const std::optional<int> r = s.wait_for_result(ms(200));
        const long long e = since(t0);
        if (!r || *r != 42) {
            std::printf("a value that was already set was not returned\n");
            return 1;
        }
        if (e > kBound) {
            std::printf("the value was already there, but wait_for_result blocked %lld ms\n", e);
            return 1;
        }
    }

    // ── 2. a value that arrives mid-wait is returned promptly ─────────────
    for (int trial = 0; trial < 8; ++trial) {
        result_slot s;
        std::thread producer([&] {
            std::this_thread::sleep_for(ms(10));
            SHAKE();
            s.set(7);
        });
        const clk::time_point t0 = clk::now();
        const std::optional<int> r = s.wait_for_result(ms(300));
        const long long e = since(t0);
        producer.join();
        if (!r || *r != 7) {
            std::printf("trial %d: the value arrived at 10 ms but wait_for_result "
                        "reported a timeout\n", trial);
            return 1;
        }
        if (e > 200) {
            std::printf("trial %d: the value arrived at 10 ms but wait_for_result "
                        "took %lld ms to return it\n", trial, e);
            return 1;
        }
    }

    // ── 3. THE POINT: wakeups must not extend the deadline ────────────────
    // A producer that keeps signalling progress and never delivers. Nothing
    // hostile about it — this is what a busy system looks like from the inside,
    // and it is indistinguishable from a run of spurious wakeups.
    //
    // A loop that hands the full timeout to every wait gives up `kTimeout` after
    // the LAST nudge, not `kTimeout` after the call. That is ~450 ms here.
    for (int trial = 0; trial < 4; ++trial) {
        result_slot s;
        std::atomic<bool> waiter_done{false};
        std::thread nudger([&] {
            const clk::time_point until = clk::now() + ms(kNudgeStop);
            while (!waiter_done.load() && clk::now() < until) {
                s.nudge();
                std::this_thread::sleep_for(ms(4));
            }
        });

        const clk::time_point t0 = clk::now();
        const std::optional<int> r = s.wait_for_result(ms(kTimeout));
        const long long e = since(t0);
        waiter_done.store(true);
        nudger.join();

        if (r) {
            std::printf("trial %d: wait_for_result returned a value that was never set\n", trial);
            return 1;
        }
        if (e > kBound) {
            std::printf("trial %d: asked to wait at most %lld ms, actually waited %lld ms.\n"
                        "The wakeups restarted the timeout instead of counting against a "
                        "deadline fixed when the call began.\n",
                        trial, kTimeout, e);
            return 1;
        }
    }

    // ── 4. many waiters, all disturbed, all must still get the value ──────
    {
        result_slot s;
        std::atomic<bool> stop{false};
        std::thread nudger([&] {
            while (!stop.load()) {
                s.nudge();
                std::this_thread::sleep_for(ms(2));
            }
        });

        std::atomic<int> got{0}, missed{0};
        std::vector<std::thread> waiters;
        for (int i = 0; i < 6; ++i) {
            waiters.emplace_back([&] {
                const std::optional<int> r = s.wait_for_result(ms(300));
                if (r && *r == 99) ++got;
                else               ++missed;
            });
        }
        std::this_thread::sleep_for(ms(20));
        SHAKE();
        s.set(99);
        for (std::thread& w : waiters) w.join();
        stop.store(true);
        nudger.join();

        if (got.load() != 6 || missed.load() != 0) {
            std::printf("6 waiters, %d got the value and %d timed out — a set() must "
                        "wake every waiter\n", got.load(), missed.load());
            return 1;
        }
    }

    // ── 5. a zero timeout is legal in both directions ─────────────────────
    {
        result_slot empty;
        const clk::time_point t0 = clk::now();
        const std::optional<int> r = empty.wait_for_result(ms(0));
        const long long e = since(t0);
        if (r) {
            std::printf("a 0 ms wait on an empty slot returned a value\n");
            return 1;
        }
        if (e > kBound) {
            std::printf("a 0 ms wait on an empty slot blocked for %lld ms\n", e);
            return 1;
        }

        result_slot full;
        full.set(5);
        const std::optional<int> r2 = full.wait_for_result(ms(0));
        if (!r2 || *r2 != 5) {
            std::printf("a 0 ms wait missed a value that was already there — the "
                        "predicate has to be checked before sleeping\n");
            return 1;
        }
    }

    std::printf("deadline honoured under continuous wakeups, value delivered to all "
                "waiters, zero timeout handled\n");
    return 0;
}
