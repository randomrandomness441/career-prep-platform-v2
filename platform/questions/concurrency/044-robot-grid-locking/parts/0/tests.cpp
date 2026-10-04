// Harness for "Robot Grid Locking". Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    // Many independent robot PAIRS, each pair sharing one adjacent cell
    // boundary. Each pair's two threads repeatedly swap into each other's
    // cell, at the same time, many times -- the exact shape that deadlocks
    // hold-and-wait grid locking: robot A holds its own cell wanting the
    // other's, robot B holds its own cell wanting A's, simultaneously.
    constexpr int kPairs = 40;
    constexpr int kRounds = 300;

    RobotCleaner grid(2, kPairs * 2);
    std::vector<std::thread> threads;

    for (int p = 0; p < kPairs; ++p) {
        int c1 = p * 2;
        int c2 = p * 2 + 1;
        threads.emplace_back([&grid, c1, c2] {
            for (int i = 0; i < kRounds; ++i) {
                SHAKE();
                grid.move(0, c1, 0, c2);
                grid.move(0, c2, 0, c1);
            }
        });
        threads.emplace_back([&grid, c1, c2] {
            for (int i = 0; i < kRounds; ++i) {
                SHAKE();
                grid.move(0, c2, 0, c1);
                grid.move(0, c1, 0, c2);
            }
        });
    }

    for (auto& t : threads) t.join();

    std::printf("%d robot pairs x %d rounds of simultaneous cell-swapping: no deadlock\n",
                kPairs, kRounds);
    return 0;
}
