// Harness for "Multithreaded Argsort". Includes the candidate's file
// verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <random>
#include <vector>

static bool check(const std::vector<int>& values, int num_buckets, int num_threads,
                   const char* label) {
    std::vector<std::size_t> perm = parallel_argsort(values, num_buckets, num_threads);
    const std::size_t n = values.size();

    if (perm.size() != n) {
        std::printf("%s: parallel_argsort returned %zu indices, expected %zu\n",
                    label, perm.size(), n);
        return false;
    }

    // perm must be a permutation of [0, n): every index appears exactly once.
    std::vector<char> seen(n, 0);
    for (std::size_t p : perm) {
        if (p >= n || seen[p]) {
            std::printf("%s: index %zu is out of range or appears more than once in the "
                        "result -- not a valid permutation (the counting pass under-counted "
                        "or over-counted a bucket)\n", label, p);
            return false;
        }
        seen[p] = 1;
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (!seen[i]) {
            std::printf("%s: original index %zu never appears in the result\n", label, i);
            return false;
        }
    }

    // values[perm[i]] must be non-decreasing.
    for (std::size_t i = 1; i < n; ++i) {
        if (values[perm[i - 1]] > values[perm[i]]) {
            std::printf("%s: not sorted at position %zu (values %d then %d)\n",
                        label, i, values[perm[i - 1]], values[perm[i]]);
            return false;
        }
    }
    return true;
}

int main() {
    std::mt19937 rng(20260909u);

    // ── 1. small, deterministic sanity check ──────────────────────────────
    {
        std::vector<int> values = {3, 1, 2, 1, 3, 2, 0, 1};
        if (!check(values, 4, 4, "small")) return 1;
    }

    // ── 2. degenerate sizes ────────────────────────────────────────────────
    {
        if (!check({}, 4, 4, "empty")) return 1;
        if (!check({0}, 4, 4, "single")) return 1;
        if (!check({2, 2, 2, 2}, 4, 4, "all-equal")) return 1;
    }

    // ── 3. large, random, real contention -- the case the naive shared
    //      histogram cannot survive ────────────────────────────────────────
    for (int trial = 0; trial < 4; ++trial) {
        constexpr std::size_t n = 200000;
        constexpr int kBuckets = 500;
        std::vector<int> values(n);
        std::uniform_int_distribution<int> dist(0, kBuckets - 1);
        for (auto& v : values) v = dist(rng);
        SHAKE();
        if (!check(values, kBuckets, 8, "large-random")) return 1;
    }

    std::printf("small, degenerate, and 4 large-random trials (200000 elements, 500 "
                "buckets, 8 threads): every result a valid, correctly sorted permutation\n");
    return 0;
}
