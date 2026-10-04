// Harness for EventGate. Includes the candidate's class verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

int main() {
    // 1. No event in progress: reg_cb runs synchronously, before it returns.
    {
        EventGate g;
        bool ran = false;
        g.reg_cb([&] { ran = true; });
        if (!ran) {
            std::printf("reg_cb with no event in progress must run synchronously\n");
            return 1;
        }
    }

    // 2. Queueing + strict FIFO order within one batch.
    {
        EventGate g;
        std::vector<int> order;
        g.begin_event();
        g.reg_cb([&] { order.push_back(1); });
        g.reg_cb([&] { order.push_back(2); });
        g.reg_cb([&] { order.push_back(3); });
        if (!order.empty()) {
            std::printf("callbacks registered during an event must not run before "
                        "end_event()\n");
            return 1;
        }
        g.end_event();
        if (order != std::vector<int>{1, 2, 3}) {
            std::printf("end_event() must run queued callbacks in registration order\n");
            return 1;
        }
    }

    // 3. Immediately after an event ends, reg_cb runs synchronously again.
    {
        EventGate g;
        g.begin_event();
        g.reg_cb([] {});
        g.end_event();
        bool ran = false;
        g.reg_cb([&] { ran = true; });
        if (!ran) {
            std::printf("reg_cb after end_event() must run synchronously, not queue\n");
            return 1;
        }
    }

    // 4. Reentrancy: a callback that calls reg_cb() from inside end_event()'s drain
    //    must not deadlock, and the re-registered callback must itself run (the event
    //    is already over by the time callbacks run, so it should execute immediately).
    {
        EventGate g;
        std::atomic<bool> inner_ran{false};
        g.begin_event();
        g.reg_cb([&] {
            g.reg_cb([&] { inner_ran.store(true); });
        });
        g.end_event();
        if (!inner_ran.load()) {
            std::printf("a callback that re-registers from inside end_event() must "
                        "still run -- possible deadlock or lost callback\n");
            return 1;
        }
    }

    // 5. Concurrent stress: many threads registering while a separate thread toggles
    //    the event on and off. Every registration must execute exactly once.
    for (int trial = 0; trial < 20; ++trial) {
        EventGate g;
        constexpr int kThreads = 6, kPerThread = 300;
        std::vector<std::atomic<int>> hit(kThreads * kPerThread);
        for (auto& h : hit) h.store(0);

        std::atomic<bool> stop_toggling{false};
        std::thread toggler([&] {
            while (!stop_toggling.load()) {
                g.begin_event();
                SHAKE();
                g.end_event();
            }
        });

        std::vector<std::thread> registrars;
        for (int t = 0; t < kThreads; ++t) {
            registrars.emplace_back([&, t] {
                for (int i = 0; i < kPerThread; ++i) {
                    const int id = t * kPerThread + i;
                    g.reg_cb([&hit, id] { hit[static_cast<std::size_t>(id)].fetch_add(1); });
                }
            });
        }
        for (auto& r : registrars) r.join();
        stop_toggling.store(true);
        toggler.join();
        g.end_event();   // drain anything still queued from the last toggle window

        for (int i = 0; i < kThreads * kPerThread; ++i) {
            const int c = hit[static_cast<std::size_t>(i)].load();
            if (c != 1) {
                std::printf("callback %d ran %d times, expected exactly 1\n", i, c);
                return 1;
            }
        }
    }

    std::printf("synchronous dispatch, queueing, FIFO drain, reentrancy and "
                "concurrent stress all correct\n");
    return 0;
}
