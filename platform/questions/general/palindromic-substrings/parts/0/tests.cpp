// Harness for count_palindromic_substrings. Includes the candidate's file verbatim.
#include "solution.hpp"
#include <cstdio>

static int fails = 0;
static void check(int got, int want, const char* name) {
    if (got != want) { std::printf("%s: got %d, expected %d\n", name, got, want); ++fails; }
}

int main() {
    check(count_palindromic_substrings(""), 0, "empty string");
    check(count_palindromic_substrings("a"), 1, "single char");
    check(count_palindromic_substrings("abc"), 3, "no repeats");
    check(count_palindromic_substrings("aaa"), 6, "all same char");
    check(count_palindromic_substrings("abba"), 6, "even-length palindrome");
    check(count_palindromic_substrings("abcba"), 7, "odd-length palindrome (whole string)");
    check(count_palindromic_substrings("aabaa"), 9, "mixed odd/even");

    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("all palindrome-counting cases correct\n");
    return 0;
}
