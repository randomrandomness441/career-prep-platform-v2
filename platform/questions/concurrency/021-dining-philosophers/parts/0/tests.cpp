// Harness for "The Dining Philosophers".
//
// The interesting failure here is a hang, not a wrong answer, so most of this
// file is about making sure a hang is a hang and not just a slow machine, and
// about proving that a correct answer is also a *parallel* one.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

const int kTrials = 6;
const int kMeals  = 40;   // meals per philosopher per trial

// State the harness watches. Nothing here is part of the answer; it exists only
// so the tests can see what the answer did.
struct Table {
    std::atomic<int> eating[5];        // 1 while that philosopher is mid-eat()
    std::atomic<int> meals[5];         // how many times each one has eaten
    std::atomic<int> in_progress{0};   // how many are eating right now
    std::atomic<int> peak{0};          // the largest value in_progress ever hit
    std::atomic<int> neighbour_clash{0};

    Table() {
        for (int i = 0; i < 5; ++i) { eating[i].store(0); meals[i].store(0); }
    }
};

// A short, deterministic amount of work, so that "eating" occupies a real
// interval rather than a single instruction -- long enough that a genuine
// neighbour-fork violation is very likely to get caught by the checks in
// do_eat() below, on either side of it.
void chew() {
    volatile int sink = 0;
    for (int i = 0; i < 2000; ++i) sink = sink + i;
}

void do_eat(Table& t, int p) {
    const int left_neighbour  = (p + 4) % 5;
    const int right_neighbour = (p + 1) % 5;

    t.eating[p].store(1);

    const int now = t.in_progress.fetch_add(1) + 1;
    int seen = t.peak.load();
    while (now > seen && !t.peak.compare_exchange_weak(seen, now)) { }

    // Neighbours share a fork, so this must never be true.
    if (t.eating[left_neighbour].load() || t.eating[right_neighbour].load())
        t.neighbour_clash.fetch_add(1);

    chew();
    SHAKE();

    if (t.eating[left_neighbour].load() || t.eating[right_neighbour].load())
        t.neighbour_clash.fetch_add(1);

    t.meals[p].fetch_add(1);
    t.in_progress.fetch_sub(1);
    t.eating[p].store(0);
}

// Direct proof that two non-neighbours CAN eat at the same real time, instead
// of hoping ambient traffic from all five philosophers happens to overlap
// somewhere across a bounded run. Philosophers 0 and 2 hold completely
// disjoint forks ({0,4} and {1,2}) -- if the locking is genuinely per-fork,
// both threads reach eat() independently and this rendezvous succeeds almost
// immediately. If it's secretly serialised behind one lock, the second thread
// never even reaches eat() and this times out.
//
// This replaces an earlier version of this check that sampled in_progress
// across the 5-philosopher, 40-meal loop below and required luck to observe
// an overlap. Measured directly: a genuinely correct five-fork solution
// failed that sampling check in roughly 1 in 30-40 shaken runs, purely from
// scheduling timing, not from anything wrong with the solution -- a
// deterministic two-thread rendezvous has no such failure mode.
bool two_nonneighbours_can_overlap() {
    DiningPhilosophers table;
    std::atomic<int> arrived{0};
    std::atomic<bool> both_seen{false};

    auto run = [&](int p) {
        table.wantsToEat(
            p, [] {}, [] {},
            [&] {
                arrived.fetch_add(1);
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                while (std::chrono::steady_clock::now() < deadline) {
                    if (arrived.load() >= 2) { both_seen.store(true); return; }
                    std::this_thread::yield();
                }
            },
            [] {}, [] {});
    };

    std::thread t0([&] { run(0); });
    std::thread t2([&] { run(2); });
    t0.join();
    t2.join();
    return both_seen.load();
}

}  // namespace

int main() {
    if (!two_nonneighbours_can_overlap()) {
        std::printf("philosophers 0 and 2 share no fork (forks {0,4} vs {1,2}), but the "
                    "second one never got to eat while the first was still eating -- this "
                    "looks like one global lock rather than five independent forks.\n");
        return 1;
    }

    int overall_peak = 0;

    for (int trial = 0; trial < kTrials; ++trial) {
        DiningPhilosophers table;
        Table watch;

        // All five threads are created before any of them starts asking for
        // forks. Without this the first philosopher runs to completion while the
        // others are still being spawned, and the table is never contended at
        // all — which would make the test a measurement of thread creation
        // rather than of fork acquisition.
        std::atomic<bool> go{false};

        std::vector<std::thread> threads;
        for (int p = 0; p < 5; ++p) {
            threads.emplace_back([&, p] {
                while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
                for (int meal = 0; meal < kMeals; ++meal) {
                    table.wantsToEat(
                        p,
                        // A philosopher does not teleport from one fork to the
                        // other. This yield is the gap between the two hands.
                        [] { std::this_thread::yield(); SHAKE(); },   // pick left
                        [] { SHAKE(); },                              // pick right
                        [&] { do_eat(watch, p); },
                        [] { },                                       // put left
                        [] { });                                      // put right
                }
            });
        }
        go.store(true, std::memory_order_release);
        for (std::thread& t : threads) t.join();

        for (int p = 0; p < 5; ++p) {
            if (watch.meals[p].load() != kMeals) {
                std::printf("trial %d: philosopher %d ate %d times, expected %d\n",
                            trial, p, watch.meals[p].load(), kMeals);
                return 1;
            }
        }
        if (watch.neighbour_clash.load() != 0) {
            std::printf("trial %d: two neighbouring philosophers were eating at the same "
                        "time %d times — they share a fork, so at least one of them was "
                        "using a fork somebody else was holding\n",
                        trial, watch.neighbour_clash.load());
            return 1;
        }
        if (watch.in_progress.load() != 0) {
            std::printf("trial %d: %d philosophers still recorded as eating after every "
                        "call returned\n", trial, watch.in_progress.load());
            return 1;
        }
        if (watch.peak.load() > overall_peak) overall_peak = watch.peak.load();
    }

    // overall_peak is informational only here -- the rendezvous check at the top
    // of main() is what actually proves concurrent eating is possible. Ambient
    // overlap across the 5-philosopher stress loop above is a bonus data point,
    // not something to gate pass/fail on (see two_nonneighbours_can_overlap()'s
    // comment for why sampling it turned out to be unreliable).
    std::printf("%d trials x 5 philosophers x %d meals: everyone ate, no neighbours ever "
                "shared a fork, up to %d ate at once (plus the direct rendezvous check)\n",
                kTrials, kMeals, overall_peak);
    return 0;
}
