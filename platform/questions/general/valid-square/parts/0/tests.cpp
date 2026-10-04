// Harness for is_valid_square. Includes the candidate's file verbatim.
#include "solution.hpp"
#include <cstdio>

static int fails = 0;
static void check(bool got, bool want, const char* name) {
    if (got != want) {
        std::printf("%s: got %s, expected %s\n", name, got ? "true" : "false",
                    want ? "true" : "false");
        ++fails;
    }
}

int main() {
    check(is_valid_square({0,0},{1,0},{1,1},{0,1}), true, "unit square, walk order");
    // Same 4 points, DIAGONAL order (p1/p3 opposite, p2/p4 opposite) -- this is the case
    // that trips up an implementation that assumes edge-walk order.
    check(is_valid_square({0,0},{1,1},{1,0},{0,1}), true, "unit square, diagonal order");
    check(is_valid_square({0,0},{0,1},{1,1},{1,0}), true, "unit square, another order");
    check(is_valid_square({0,0},{0,0},{0,0},{0,0}), false, "four coincident points");
    check(is_valid_square({0,0},{2,0},{2,1},{0,1}), false, "2x1 rectangle");
    check(is_valid_square({0,0},{1,1},{2,0},{1,-1}), true, "rotated square (diamond)");
    check(is_valid_square({0,0},{1,0},{2,0},{3,0}), false, "four collinear points");
    check(is_valid_square({-1,-1},{-1,1},{1,1},{1,-1}), true, "square around the origin");

    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("all square-detection cases correct\n");
    return 0;
}
