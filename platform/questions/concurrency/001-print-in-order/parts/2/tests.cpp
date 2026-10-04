// Follow-up 2: N threads, strict ordering, one mutex + one CV.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

int main() {
    std::mt19937 rng(4242);

    for (int trial = 0; trial < 8; ++trial) {
        const int N = 100;
        Ordered ord(N);
        std::vector<int> seen;
        std::mutex om;

        std::vector<int> ids(N);
        std::iota(ids.begin(), ids.end(), 1);
        std::shuffle(ids.begin(), ids.end(), rng);   // launch order is scrambled

        std::vector<std::thread> ts;
        ts.reserve(N);
        for (int id : ids) {
            ts.emplace_back([&, id] {
                SHAKE();
                ord.go(id, [&, id] {
                    std::lock_guard<std::mutex> g(om);
                    seen.push_back(id);
                });
            });
        }
        for (auto& t : ts) t.join();

        if ((int)seen.size() != N) {
            std::printf("trial %d: expected %d prints, got %zu\n", trial, N, seen.size());
            return 1;
        }
        for (int i = 0; i < N; ++i) {
            if (seen[i] != i + 1) {
                std::printf("trial %d: position %d holds id %d, expected %d\n",
                            trial, i, seen[i], i + 1);
                return 1;
            }
        }
    }
    std::printf("8 trials x 100 threads, strict order held every time\n");
    return 0;
}
