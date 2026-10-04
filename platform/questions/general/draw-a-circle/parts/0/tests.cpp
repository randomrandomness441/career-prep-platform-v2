// Harness for circle_points. Includes the candidate's file verbatim.
#include "solution.hpp"
#include <cstdio>
#include <set>

int main() {
    // 1. r == 0 is just the center.
    {
        auto pts = circle_points(3, 3, 0);
        std::set<std::pair<int,int>> got(pts.begin(), pts.end());
        if (got != std::set<std::pair<int,int>>{{3,3}}) {
            std::printf("r=0 should return just the center point\n");
            return 1;
        }
    }

    // 2. No duplicates, for a small radius.
    {
        auto pts = circle_points(0, 0, 5);
        std::set<std::pair<int,int>> got(pts.begin(), pts.end());
        if (got.size() != pts.size()) {
            std::printf("circle_points(0,0,5) returned duplicate coordinates\n");
            return 1;
        }
    }

    // 3. Exact match against the true outline at r=20 -- large enough that a fixed
    //    angular step under-covers it (gaps), while the integer midpoint algorithm
    //    doesn't.
    {
        auto pts = circle_points(0, 0, 20);
        std::set<std::pair<int,int>> got(pts.begin(), pts.end());
        std::set<std::pair<int,int>> want = {
            {-20,-4},{-20,-3},{-20,-2},{-20,-1},{-20,0},{-20,1},{-20,2},{-20,3},{-20,4},
            {-19,-7},{-19,-6},{-19,-5},{-19,5},{-19,6},{-19,7},
            {-18,-9},{-18,-8},{-18,8},{-18,9},
            {-17,-11},{-17,-10},{-17,10},{-17,11},
            {-16,-12},{-16,12},{-15,-13},{-15,13},{-14,-14},{-14,14},
            {-13,-15},{-13,15},{-12,-16},{-12,16},{-11,-17},{-11,17},
            {-10,-17},{-10,17},{-9,-18},{-9,18},{-8,-18},{-8,18},
            {-7,-19},{-7,19},{-6,-19},{-6,19},{-5,-19},{-5,19},
            {-4,-20},{-4,20},{-3,-20},{-3,20},{-2,-20},{-2,20},{-1,-20},{-1,20},
            {0,-20},{0,20},{1,-20},{1,20},{2,-20},{2,20},{3,-20},{3,20},{4,-20},{4,20},
            {5,-19},{5,19},{6,-19},{6,19},{7,-19},{7,19},
            {8,-18},{8,18},{9,-18},{9,18},
            {10,-17},{10,17},{11,-17},{11,17},
            {12,-16},{12,16},{13,-15},{13,15},{14,-14},{14,14},
            {15,-13},{15,13},{16,-12},{16,12},
            {17,-11},{17,-10},{17,10},{17,11},
            {18,-9},{18,-8},{18,8},{18,9},
            {19,-7},{19,-6},{19,-5},{19,5},{19,6},{19,7},
            {20,-4},{20,-3},{20,-2},{20,-1},{20,0},{20,1},{20,2},{20,3},{20,4},
        };
        if (got.size() != want.size()) {
            std::printf("circle_points(0,0,20): got %zu points, expected %zu\n",
                        got.size(), want.size());
            return 1;
        }
        for (const auto& p : want) {
            if (!got.count(p)) {
                std::printf("circle_points(0,0,20) is missing point (%d,%d)\n", p.first, p.second);
                return 1;
            }
        }
    }

    std::printf("center-only, no-duplicates and full-outline cases all correct\n");
    return 0;
}
