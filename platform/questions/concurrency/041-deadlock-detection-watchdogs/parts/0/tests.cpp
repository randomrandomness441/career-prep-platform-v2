// Harness for "Deadlock Detection & Watchdogs".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

int main() {
    // ── 1. a plain stall, no second thread involved: the deadline itself ──
    // op() sleeps far longer than the deadline. This alone tells the naive
    // version and the correct one apart, fast: the naive version ignores
    // the deadline and blocks for the full sleep, then WRONGLY reports
    // success. No hang, no timeout needed to catch this.
    {
        Watchdog wd;
        auto t0 = std::chrono::steady_clock::now();
        bool completed = wd.run_with_deadline([] { std::this_thread::sleep_for(300ms); }, 50ms);
        auto elapsed = std::chrono::steady_clock::now() - t0;
        if (completed) {
            std::printf("a 300ms operation given a 50ms deadline was reported as "
                        "completed -- the deadline was never checked\n");
            return 1;
        }
        if (elapsed > 250ms) {
            std::printf("run_with_deadline took %lldms to report a stall against a "
                        "50ms deadline -- it waited for the operation instead of the "
                        "deadline\n", (long long)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
            return 1;
        }
    }

    // ── 2. a fast operation must still be reported as completed ───────────
    {
        Watchdog wd;
        std::atomic<bool> ran{false};
        bool completed = wd.run_with_deadline([&] { ran.store(true); }, 200ms);
        if (!completed || !ran.load()) {
            std::printf("a fast operation with a generous deadline was not reported "
                        "as completed\n");
            return 1;
        }
    }

    // ── 3. a real deadlock: two mutexes, opposite acquisition order ───────
    // op_ab locks A then B; op_ba locks B then A. Run concurrently, with a
    // sleep between the two locks to widen the window, this is the same
    // shape as the hierarchical-mutex and dining-philosophers questions --
    // a genuine, permanent deadlock, not a slow operation. Both watchdogs
    // must detect the stall and report it; neither call may hang the test.
    //
    // A correct Watchdog detaches the thread stuck in a real deadlock -- by
    // design, since there is no safe way to force-stop it -- so that thread
    // keeps running forever, still holding a lock_guard on A and B. A and B
    // must therefore never be destroyed: leak a pair per trial on the heap
    // on purpose, rather than let a stack-allocated pair unwind out from
    // under a thread that will reference it for the rest of the process's
    // life. This is not a mistake to clean up; it's the point.
    for (int trial = 0; trial < 5; ++trial) {
        auto* A = new std::mutex;
        auto* B = new std::mutex;
        auto op_ab = [A, B] {
            std::lock_guard<std::mutex> l1(*A);
            std::this_thread::sleep_for(25ms);
            std::lock_guard<std::mutex> l2(*B);
        };
        auto op_ba = [A, B] {
            std::lock_guard<std::mutex> l1(*B);
            std::this_thread::sleep_for(25ms);
            std::lock_guard<std::mutex> l2(*A);
        };

        Watchdog wd1, wd2;
        bool r1 = true, r2 = true;
        SHAKE();
        std::thread caller1([&] { r1 = wd1.run_with_deadline(op_ab, 200ms); });
        std::thread caller2([&] { r2 = wd2.run_with_deadline(op_ba, 200ms); });
        caller1.join();
        caller2.join();

        if (r1 || r2) {
            std::printf("trial %d: a real A/B-vs-B/A deadlock was not detected "
                        "(r1=%d r2=%d) -- the watchdog reported success on an "
                        "operation that never finished\n", trial, r1, r2);
            return 1;
        }
    }

    // ── 4. legitimate contention on the SAME two mutexes must never
    //       false-trigger, as long as every caller locks in the same order ─
    for (int trial = 0; trial < 30; ++trial) {
        std::mutex A, B;
        auto op = [&] {
            std::lock_guard<std::mutex> l1(A);
            std::this_thread::sleep_for(2ms);
            std::lock_guard<std::mutex> l2(B);
        };

        Watchdog wd1, wd2;
        bool r1 = false, r2 = false;
        std::thread caller1([&] { r1 = wd1.run_with_deadline(op, 100ms); });
        std::thread caller2([&] { r2 = wd2.run_with_deadline(op, 100ms); });
        caller1.join();
        caller2.join();

        if (!r1 || !r2) {
            std::printf("trial %d: consistent lock ordering (no deadlock possible) "
                        "still triggered the watchdog (r1=%d r2=%d) -- a false "
                        "positive\n", trial, r1, r2);
            return 1;
        }
    }

    std::printf("timeout without a deadlock, fast success, %d real-deadlock trials "
                "all detected, and %d contended-but-safe trials with zero false "
                "positives\n", 5, 30);
    return 0;
}
