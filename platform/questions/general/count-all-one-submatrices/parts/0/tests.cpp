// Harness for count_submatrices. Includes the candidate's file verbatim.
#include "solution.hpp"
#include <cstdio>

static int fails = 0;
static void check(long long got, long long want, const char* name) {
    if (got != want) { std::printf("%s: got %lld, expected %lld\n", name, got, want); ++fails; }
}

int main() {
    check(count_submatrices({{1,0,1},{1,1,0},{1,1,0}}), 13, "classic example");
    check(count_submatrices({{1,1,1,1,1,1}}), 21, "single row, all ones");
    check(count_submatrices({{0,0},{0,0}}), 0, "all zeros");
    check(count_submatrices({{1}}), 1, "single cell, one");
    check(count_submatrices({{0}}), 0, "single cell, zero");
    // A 0 breaking a column's run must actually break it, not just pause it.
    check(count_submatrices({{1,1},{0,0},{1,1}}), 6, "0-row splits two separate runs");

    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("all submatrix-counting cases correct\n");
    return 0;
}
