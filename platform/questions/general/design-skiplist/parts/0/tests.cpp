// Harness for Skiplist. Includes the candidate's class verbatim.
#include "solution.hpp"
#include <cstdio>
#include <vector>

int main() {
    // 1. Basic search/add/erase from the problem statement's own example.
    {
        Skiplist sl;
        sl.add(1);
        sl.add(2);
        sl.add(3);
        if (sl.search(0)) { std::printf("search(0) should be false before any 0 is added\n"); return 1; }
        sl.add(4);
        if (!sl.search(1)) { std::printf("search(1) should be true after add(1)\n"); return 1; }
        if (sl.erase(0)) { std::printf("erase(0) should be false -- 0 was never added\n"); return 1; }
        if (!sl.erase(1)) { std::printf("erase(1) should succeed\n"); return 1; }
        if (sl.search(1)) { std::printf("search(1) should be false after erase(1)\n"); return 1; }
    }

    // 2. Duplicates: adding the same value twice, erasing once, must leave one copy.
    {
        Skiplist sl;
        sl.add(5);
        sl.add(5);
        if (!sl.erase(5)) { std::printf("erase should find the duplicate\n"); return 1; }
        if (!sl.search(5)) { std::printf("one copy of 5 should remain after erasing one of two\n"); return 1; }
        if (!sl.erase(5)) { std::printf("second erase should also succeed\n"); return 1; }
        if (sl.search(5)) { std::printf("no copies of 5 should remain\n"); return 1; }
    }

    // 3. Many insertions (virtually guarantees several nodes get promoted above level 0
    //    with the fixed seed used here), then erase every one of them and confirm none
    //    are still findable -- this is what a level-0-only erase gets wrong.
    {
        Skiplist sl;
        std::vector<int> values;
        for (int i = 0; i < 300; ++i) {
            sl.add(i);
            values.push_back(i);
        }
        for (int v : values) {
            if (!sl.erase(v)) {
                std::printf("erase(%d) should succeed -- it was added\n", v);
                return 1;
            }
        }
        for (int v : values) {
            if (sl.search(v)) {
                std::printf("search(%d) found a value after every copy was erased -- "
                            "stale pointer at a level above 0\n", v);
                return 1;
            }
        }
    }

    // 4. Interleaved add/search/erase, still exercising higher levels.
    {
        Skiplist sl;
        for (int i = 0; i < 200; i += 2) sl.add(i);
        for (int i = 0; i < 200; i += 2) {
            if (!sl.search(i)) { std::printf("search(%d) should be true\n", i); return 1; }
            if (sl.search(i + 1)) { std::printf("search(%d) should be false\n", i + 1); return 1; }
        }
        for (int i = 0; i < 200; i += 4) sl.erase(i);
        for (int i = 0; i < 200; i += 4) {
            if (sl.search(i)) { std::printf("search(%d) should be false after erase\n", i); return 1; }
        }
        for (int i = 2; i < 200; i += 4) {
            if (!sl.search(i)) { std::printf("search(%d) should still be true\n", i); return 1; }
        }
    }

    std::printf("search, add, erase, duplicates and multi-level cleanup all correct\n");
    return 0;
}
