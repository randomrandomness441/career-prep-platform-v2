// Harness for FastSet. Includes the candidate's class verbatim.
#include "solution.hpp"
#include <cstdio>
#include <algorithm>
#include <vector>

int main() {
    // 1. Basics.
    {
        FastSet s(10);
        if (s.contains(3)) { std::printf("nothing inserted yet, contains(3) should be false\n"); return 1; }
        s.insert(3);
        s.insert(7);
        if (!s.contains(3) || !s.contains(7)) { std::printf("inserted elements should be found\n"); return 1; }
        if (s.contains(5)) { std::printf("never-inserted element 5 should not be found\n"); return 1; }
        s.remove(3);
        if (s.contains(3)) { std::printf("removed element should no longer be found\n"); return 1; }
        s.remove(100000);   // out of range would be a different bug class; keep in range
    }

    // 2. The stale sparse-pointer case: this is the one a missing double-check gets
    //    wrong. Insert 10 elements, remove one from the middle (leaves a stale entry
    //    behind for the removed value), then probe several never-inserted values.
    {
        FastSet s(20);
        for (int i = 0; i < 10; ++i) s.insert(i);
        s.remove(5);   // 5's old sparse_[] slot is now stale
        if (s.contains(5)) {
            std::printf("removed element 5 should not be found (stale sparse-array entry)\n");
            return 1;
        }
        for (int x = 10; x < 20; ++x) {
            if (s.contains(x)) {
                std::printf("never-inserted element %d should not be found "
                            "(a default/stale sparse-array entry pointing inside the "
                            "current size, without checking the dense array, looks "
                            "like a false match)\n", x);
                return 1;
            }
        }
    }

    // 3. clear() actually clears, and the set is fully usable afterward.
    {
        FastSet s(50);
        for (int i = 0; i < 50; ++i) s.insert(i);
        s.clear();
        for (int i = 0; i < 50; ++i) {
            if (s.contains(i)) { std::printf("element %d should not survive clear()\n", i); return 1; }
        }
        s.insert(4);
        s.insert(9);
        std::vector<int> v = s.to_vector();
        std::sort(v.begin(), v.end());
        if (v != std::vector<int>{4, 9}) { std::printf("to_vector() wrong after clear()+reinsert\n"); return 1; }
    }

    // 4. Duplicate insert / redundant remove are no-ops.
    {
        FastSet s(5);
        s.insert(2);
        s.insert(2);
        s.insert(2);
        if (s.to_vector().size() != 1) { std::printf("duplicate insert should not duplicate\n"); return 1; }
        s.remove(3);
        s.remove(3);
        if (!s.contains(2)) { std::printf("unrelated remove() should not disturb existing elements\n"); return 1; }
    }

    std::printf("insert, remove, contains, clear and to_vector all correct\n");
    return 0;
}
