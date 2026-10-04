// Harness for "Parallel Prefix Sum (Scan)". Includes the candidate's file
// verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <random>
#include <vector>

static bool check(const std::vector<long>& input, int num_threads, const char* label) {
    std::vector<long> got = parallel_prefix_sum(input, num_threads);
    if (got.size() != input.size()) {
        std::printf("%s: result has %zu elements, expected %zu\n",
                    label, got.size(), input.size());
        return false;
    }
    long running = 0;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (got[i] != running) {
            std::printf("%s: result[%zu] = %ld, expected %ld (exclusive prefix sum)\n",
                        label, i, got[i], running);
            return false;
        }
        running += input[i];
    }
    return true;
}

int main() {
    std::mt19937 rng(20260909u);

    // ── 1. small, deterministic ────────────────────────────────────────────
    if (!check({1, 2, 3, 4, 5}, 4, "small")) return 1;
    if (!check({}, 4, "empty")) return 1;
    if (!check({7}, 4, "single")) return 1;
    if (!check({0, 0, 0, 0}, 4, "all-zero")) return 1;
    if (!check({-3, 5, -2, 8, -1}, 4, "negatives")) return 1;

    // ── 2. thread count doesn't divide the array evenly ────────────────────
    {
        std::vector<long> v(23);
        for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<long>(i) + 1;
        if (!check(v, 5, "uneven-chunks")) return 1;
    }

    // ── 3. large, random ────────────────────────────────────────────────────
    for (int trial = 0; trial < 4; ++trial) {
        constexpr std::size_t n = 500000;
        std::vector<long> v(n);
        std::uniform_int_distribution<long> dist(-1000, 1000);
        for (auto& x : v) x = dist(rng);
        SHAKE();
        if (!check(v, 8, "large-random")) return 1;
    }

    std::printf("small/degenerate cases, uneven chunk division, and 4 large-random "
                "trials (500000 elements, 8 threads): every prefix sum exact\n");
    return 0;
}
