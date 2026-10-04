#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <type_traits>
#include <vector>

static_assert(!std::is_copy_constructible<joining_thread>::value,
              "joining_thread must not be copyable");
static_assert(!std::is_copy_assignable<joining_thread>::value,
              "joining_thread must not be copy-assignable");
static_assert(std::is_move_constructible<joining_thread>::value,
              "joining_thread must be move-constructible");
static_assert(std::is_move_assignable<joining_thread>::value,
              "joining_thread must be move-assignable");

int main() {
    // 1. the destructor joins
    {
        std::atomic<int> done{0};
        { joining_thread jt([&]{ SHAKE(); done.store(1); }); }
        if (done.load() != 1) { std::printf("destructor did not join\n"); return 1; }
    }

    // 2. movable into a container
    {
        std::atomic<int> count{0};
        {
            std::vector<joining_thread> pool;
            for (int i = 0; i < 8; ++i)
                pool.emplace_back([&]{ SHAKE(); ++count; });
        }
        if (count.load() != 8) {
            std::printf("vector of threads: count = %d, expected 8\n", count.load());
            return 1;
        }
    }

    // 3. move-assignment over a LIVE thread must join it, not drop it
    {
        std::atomic<int> a_done{0}, b_done{0};
        {
            joining_thread a([&]{ SHAKE(); a_done.store(1); });
            joining_thread b([&]{ SHAKE(); b_done.store(1); });
            a = std::move(b);          // must join a's original thread first
            if (a_done.load() != 1) {
                std::printf("move-assign dropped the original thread without joining\n");
                return 1;
            }
        }
        if (b_done.load() != 1) { std::printf("moved-in thread never joined\n"); return 1; }
    }

    // 4. self-move must survive
    {
        std::atomic<int> done{0};
        {
            joining_thread a([&]{ SHAKE(); done.store(1); });
            a = std::move(a);
        }
        if (done.load() != 1) { std::printf("self-move broke the object\n"); return 1; }
    }

    std::printf("destructor joins, movable, non-copyable, move-assign and self-move safe\n");
    return 0;
}
