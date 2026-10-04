// Harness for dedupe(). Includes the candidate's file verbatim.
#include "solution.hpp"
#include <cstdio>
#include <chrono>
#include <unordered_set>

static int fails = 0;

static void expect_eq(const std::vector<int>& got, const std::vector<int>& want,
                       const char* name) {
    bool ok = got.size() == want.size();
    for (size_t i = 0; ok && i < got.size(); i++) ok = got[i] == want[i];
    if (!ok) {
        std::printf("%s: wrong output (size got=%zu want=%zu)\n", name, got.size(), want.size());
        ++fails;
    }
}

int main() {
    // Small, direct correctness checks: first-occurrence order, not sorted order.
    expect_eq(dedupe({}), {}, "empty input");
    expect_eq(dedupe({1, 2, 3}), {1, 2, 3}, "no duplicates");
    expect_eq(dedupe({5, 5, 5, 5}), {5}, "all duplicates");
    expect_eq(dedupe({3, 1, 3, 2, 1}), {3, 1, 2}, "duplicates keep first-occurrence order");

    // Large-scale correctness + a real time budget. The input is generated with a
    // fixed linear-congruential sequence, not std::rand(), so it's identical on every
    // run -- no flakiness from run to run.
    const int N = 150000;
    std::vector<int> ids;
    ids.reserve(N);
    unsigned x = 12345;
    for (int i = 0; i < N; i++) {
        x = x * 1103515245u + 12345u;
        ids.push_back((int)(x % (unsigned)N));
    }

    // Independent reference computation, not calling dedupe() itself, so this doesn't
    // just check "did you return the input unchanged."
    std::vector<int> expected;
    std::unordered_set<int> seen_ref;
    expected.reserve(N);
    for (int id : ids) {
        if (seen_ref.insert(id).second) expected.push_back(id);
    }

    auto t0 = std::chrono::steady_clock::now();
    std::vector<int> got = dedupe(ids);
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    expect_eq(got, expected, "large input, correctness");

    // 150,000 elements, correct implementations finish in well under 25ms (measured
    // on this machine across -O1, -O2, and under ThreadSanitizer instrumentation).
    // An O(n^2) implementation takes over a second at this size -- this budget is
    // wide enough to never punish a correct approach for machine noise, and narrow
    // enough that nothing quadratic gets through.
    const double BUDGET_MS = 300.0;
    if (ms > BUDGET_MS) {
        std::printf("dedupe(%d ids) took %.1fms, must be under %.0fms.\n"
                     "That gap is what a profiler would have shown you before you ever\n"
                     "got here -- something in this function is doing far more work per\n"
                     "element than it looks like from reading it.\n", N, ms, BUDGET_MS);
        ++fails;
    } else {
        std::printf("large input: correct and %.1fms (budget %.0fms)\n", ms, BUDGET_MS);
    }

    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("all checks passed\n");
    return 0;
}
