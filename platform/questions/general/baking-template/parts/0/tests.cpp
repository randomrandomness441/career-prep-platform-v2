// Harness for count_matching_pairs. Includes the candidate's file verbatim.
#include "solution.hpp"
#include <cstdio>

static int fails = 0;
static void check(int got, int want, const char* name) {
    if (got != want) { std::printf("%s: got %d, expected %d\n", name, got, want); ++fails; }
}

int main() {
    check(count_matching_pairs({{"cm","mc"},{"ccm","mcc"},{"pc","mc"},{"pmc","mcp"}}),
          3, "the reported example");
    check(count_matching_pairs({{"cm","ccm"}}), 0, "same letters, different counts");
    check(count_matching_pairs({{"",""}}), 1, "two empty strings match");
    check(count_matching_pairs({{"abc","cab"}}), 1, "pure anagram");
    check(count_matching_pairs({{"aab","abb"}}), 0, "same set, different counts, no repeats across");

    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("all box/template matching cases correct\n");
    return 0;
}
