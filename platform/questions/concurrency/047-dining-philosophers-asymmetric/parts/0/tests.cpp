// Harness for "Dining Philosophers with Asymmetric Resource Starvation".
// Includes the candidate's file verbatim.
#include "solution.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    // ── 1. basic sanity: it must still be deadlock-free and mutually
    // exclusive on shared forks -- unmodified from 021's own check, just
    // brief, since this question's real subject is fairness, not this.
    {
        std::atomic<int> eating[5];
        for (auto& e : eating) e.store(0);
        std::atomic<int> clashes{0};

        DiningPhilosophers table;
        std::vector<std::thread> threads;
        for (int p = 0; p < 5; ++p) {
            threads.emplace_back([&, p] {
                for (int i = 0; i < 20; ++i) {
                    table.wantsToEat(
                        p, [] {}, [] {},
                        [&] {
                            eating[p].store(1);
                            int l = (p + 4) % 5, r = (p + 1) % 5;
                            if (eating[l].load() || eating[r].load()) clashes.fetch_add(1);
                            eating[p].store(0);
                        },
                        [] {}, [] {});
                }
            });
        }
        for (auto& t : threads) t.join();
        if (clashes.load() != 0) {
            std::printf("neighbouring philosophers shared a fork %d times\n", clashes.load());
            return 1;
        }
    }

    // ── 2. the actual question: philosopher 0 is "10x hungrier" -- it
    // calls wantsToEat() back-to-back with no delay. Neighbours "think"
    // for 200us between meals, a realistic gap. Run for a fixed window and
    // compare meal counts: philosopher 0 getting SOME edge is expected and
    // fine (it really is asking more often); getting many THOUSANDS of
    // times more than its neighbours combined is the actual bug.
    {
        DiningPhilosophers table;
        std::atomic<bool> stop{false};
        std::atomic<long> meals[5];
        for (auto& m : meals) m.store(0);

        std::vector<std::thread> threads;
        for (int p = 0; p < 5; ++p) {
            threads.emplace_back([&, p] {
                while (!stop.load(std::memory_order_relaxed)) {
                    table.wantsToEat(
                        p, [] {}, [] {},
                        [&] { meals[p].fetch_add(1, std::memory_order_relaxed); },
                        [] {}, [] {});
                    if (p != 0) std::this_thread::sleep_for(std::chrono::microseconds(200));
                }
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        stop.store(true, std::memory_order_relaxed);
        for (auto& t : threads) t.join();

        long neighbour_total = 0;
        for (int p = 1; p < 5; ++p) neighbour_total += meals[p].load();
        double neighbour_avg = neighbour_total / 4.0;
        long hungry = meals[0].load();

        if (neighbour_avg <= 0) {
            std::printf("the neighbours got zero meals in 150ms -- that's outright starvation, "
                        "not just an imbalance\n");
            return 1;
        }
        double ratio = hungry / neighbour_avg;
        if (ratio > 100.0) {
            std::printf("philosopher 0 ate %ld times; neighbours averaged %.1f each -- a "
                        "%.0fx ratio. Philosopher 0 asking more often should mean SOME edge, "
                        "not a many-thousand-times advantage. Nothing here bounds how often "
                        "one caller can win the race back into the fork mutexes.\n",
                        hungry, neighbour_avg, ratio);
            return 1;
        }
    }

    std::printf("no fork ever shared between neighbours, and the hungry philosopher's "
                "advantage over its neighbours stayed bounded, not runaway\n");
    return 0;
}
