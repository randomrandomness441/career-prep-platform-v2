// Harness for "Traffic Light Controlled Intersection".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

using us = std::chrono::microseconds;

constexpr int kNs = 0;   // north/south axis
constexpr int kEw = 1;   // east/west axis

}  // namespace

int main() {
    // ── Phase 1: safety and liveness under mixed load ──────────────────
    // Cars arrive on both axes at once, many times. Every car must cross,
    // and two cars from different axes must never be on the bridge at the
    // same moment.
    constexpr int kTrials      = 24;
    constexpr int kCarsPerAxis = 8;
    for (int trial = 0; trial < kTrials; ++trial) {
        traffic_light tl;
        std::atomic<int> on_bridge[2] = {0, 0};
        std::atomic<int> crossed[2]   = {0, 0};
        std::atomic<bool> clash{false};

        std::vector<std::thread> cars;
        cars.reserve(2 * kCarsPerAxis);
        for (int axis = 0; axis < 2; ++axis) {
            for (int i = 0; i < kCarsPerAxis; ++i) {
                cars.emplace_back([&tl, &on_bridge, &crossed, &clash, axis] {
                    tl.cross(axis, [&on_bridge, &crossed, &clash, axis] {
                        SHAKE();
                        on_bridge[axis].fetch_add(1);
                        if (on_bridge[0].load() > 0 && on_bridge[1].load() > 0)
                            clash.store(true);
                        std::this_thread::sleep_for(us(120));
                        on_bridge[axis].fetch_sub(1);
                        crossed[axis].fetch_add(1);
                    });
                });
            }
        }
        for (std::thread& t : cars) t.join();

        if (clash.load()) {
            std::printf("trial %d: cars from both axes were on the bridge at once\n", trial);
            return 1;
        }
        for (int axis = 0; axis < 2; ++axis) {
            if (crossed[axis].load() != kCarsPerAxis) {
                std::printf("trial %d: axis %d crossed %d of %d cars\n",
                            trial, axis, crossed[axis].load(), kCarsPerAxis);
                return 1;
            }
            if (on_bridge[axis].load() != 0) {
                std::printf("trial %d: axis %d left %d cars on the bridge\n",
                            trial, axis, on_bridge[axis].load());
                return 1;
            }
        }
    }

    // ── Phase 2: cars on the SAME axis cross together ──────────────────
    // The whole point of a light instead of a stop sign: 8 cars on one
    // axis, each needing 1 ms on the bridge, must overlap. A version that
    // holds its lock through the crossing serialises them (peak 1).
    {
        traffic_light tl;
        std::atomic<int> active{0};
        std::atomic<int> peak{0};
        std::vector<std::thread> cars;
        for (int i = 0; i < 8; ++i) {
            cars.emplace_back([&] {
                tl.cross(kNs, [&] {
                    const int now = active.fetch_add(1) + 1;
                    if (now > peak.load()) peak.store(now);
                    std::this_thread::sleep_for(us(1000));
                    active.fetch_sub(1);
                });
            });
        }
        for (std::thread& t : cars) t.join();
        if (peak.load() < 4) {
            std::printf("same-axis cars did not cross together: peak %d of 8 on the bridge\n",
                        peak.load());
            return 1;
        }
    }

    // ── Phase 3: the axis closes, the light hands over ─────────────────
    // Six N/S cars take the bridge (60 ms each). At +10ms an E/W car
    // arrives and queues. At +20ms one more N/S car arrives. The late N/S
    // car may NOT enter the bridge before the E/W car does: once the other
    // axis is waiting, the green axis is closed to newcomers. This is the
    // rule that keeps an endless N/S stream from starving E/W forever.
    {
        traffic_light tl;
        std::atomic<long long> ns_entries{0};   // N/S cars that got onto the bridge
        std::atomic<long long> ew_entry_seq{0}; // ns_entries as seen when E/W entered
        std::atomic<bool> ew_crossed{false};

        std::vector<std::thread> batch;
        for (int i = 0; i < 6; ++i) {
            batch.emplace_back([&] {
                tl.cross(kNs, [&] {
                    ns_entries.fetch_add(1);
                    std::this_thread::sleep_for(us(60000));
                });
            });
        }
        std::this_thread::sleep_for(us(10000));   // the six are on the bridge

        std::thread ew([&] {
            tl.cross(kEw, [&] {
                ew_entry_seq.store(ns_entries.load());
                std::this_thread::sleep_for(us(100));
                ew_crossed.store(true);
            });
        });
        std::this_thread::sleep_for(us(10000));   // the E/W car is queued now

        std::thread late_ns([&] {
            tl.cross(kNs, [&] { std::this_thread::sleep_for(us(100)); });
        });
        std::this_thread::sleep_for(us(20000));   // late N/S would be on the
                                                  // bridge by now if allowed
        const long long mark = ns_entries.load();
        ew.join();
        if (!ew_crossed.load()) { std::printf("the E/W car never crossed\n"); return 1; }

        if (ew_entry_seq.load() > mark) {
            std::printf("starved handover: %lld N/S cars entered the bridge while "
                        "the E/W car was waiting (expected 0)\n",
                        ew_entry_seq.load() - mark);
            late_ns.join();
            return 1;
        }
        late_ns.join();
        for (std::thread& t : batch) t.join();
    }

    std::printf("no axis clash in %d mixed trials, same-axis peak >= 4, "
                "axis closes for a waiting opponent\n", kTrials);
    return 0;
}
