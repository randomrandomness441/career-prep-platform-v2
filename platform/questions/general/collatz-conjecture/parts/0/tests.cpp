// Harness for collatz_steps. Includes the candidate's file verbatim.
#include "solution.hpp"
#include <cstdio>

static int fails = 0;
static void check(long long got, long long want, const char* name) {
    if (got != want) { std::printf("%s: got %lld, expected %lld\n", name, got, want); ++fails; }
}

int main() {
    check(collatz_steps(1), 0, "n=1");
    check(collatz_steps(2), 1, "n=2");
    check(collatz_steps(6), 8, "n=6");
    check(collatz_steps(27), 111, "n=27, the famous one that climbs to 9232");
    // n fits comfortably in a 32-bit int, but the SEQUENCE overflows one long before
    // it comes back down -- this is the case that separates a long-long-throughout
    // implementation from one that quietly narrows to int somewhere in the loop.
    check(collatz_steps(715827883), 190, "n well within int range, sequence overflows int");

    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("all Collatz sequence lengths correct\n");
    return 0;
}
