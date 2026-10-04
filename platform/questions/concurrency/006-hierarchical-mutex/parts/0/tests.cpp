#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

// Did an attempt to lock `m` while already holding a lower-or-equal level throw?
bool throws_on_lock(hierarchical_mutex& m) {
    try {
        m.lock();
        m.unlock();          // it did not throw: undo, so the test can continue
        return false;
    } catch (const std::logic_error&) {
        return true;
    }
}

bool throws_on_try_lock(hierarchical_mutex& m) {
    try {
        if (m.try_lock()) m.unlock();
        return false;
    } catch (const std::logic_error&) {
        return true;
    }
}

}  // namespace

int main() {
    // 1. It is a Lockable, so the standard RAII wrappers work with it.
    {
        hierarchical_mutex high(10000);
        std::lock_guard<hierarchical_mutex> lk(high);
        SHAKE();
    }
    {
        hierarchical_mutex solo(500);
        std::unique_lock<hierarchical_mutex> lk(solo, std::try_to_lock);
        if (!lk.owns_lock()) {
            std::printf("try_lock on an uncontended mutex failed\n");
            return 1;
        }
    }

    // 2. Descending order is legal, at any depth, and unlocking unwinds cleanly.
    for (int trial = 0; trial < 200; ++trial) {
        hierarchical_mutex high(10000), mid(5000), low(1000);
        try {
            high.lock();
            SHAKE();
            mid.lock();
            low.lock();
            low.unlock();
            mid.unlock();
            high.unlock();
        } catch (const std::logic_error& e) {
            std::printf("descending order 10000 -> 5000 -> 1000 was rejected: %s\n", e.what());
            return 1;
        }
        // Back to holding nothing, so the top of the hierarchy is available again.
        if (throws_on_lock(high)) {
            std::printf("unlock did not restore the thread's level: "
                        "re-locking the top-level mutex was rejected\n");
            return 1;
        }
    }

    // 3. THE POINT: ascending order must be reported, not silently allowed.
    for (int trial = 0; trial < 200; ++trial) {
        hierarchical_mutex high(10000), low(1000);
        low.lock();
        SHAKE();
        if (!throws_on_lock(high)) {
            std::printf("locking 1000 then 10000 was allowed — the hierarchy is not "
                        "being checked, so a real lock-order inversion would deadlock "
                        "instead of throwing\n");
            return 1;
        }
        if (!throws_on_try_lock(high)) {
            std::printf("try_lock ignored the hierarchy: 1000 then 10000 succeeded\n");
            return 1;
        }
        // The failed attempt must leave the thread exactly as it was: still
        // holding `low`, still able to unlock it and start over.
        low.unlock();
        if (throws_on_lock(high)) {
            std::printf("a rejected lock() corrupted the thread's level\n");
            return 1;
        }
    }

    // 4. Equal levels are a violation too — nothing orders two mutexes of the
    //    same level, so a pair of them is a deadlock waiting to happen.
    {
        hierarchical_mutex a(5000), b(5000);
        a.lock();
        if (!throws_on_lock(b)) {
            std::printf("two mutexes at the same level (5000) were both locked; "
                        "the check must be strictly less-than\n");
            return 1;
        }
        a.unlock();
    }

    // 5. The level is per-thread. Eight threads walking the same hierarchy at the
    //    same time must not see each other's numbers, and the mutexes must still
    //    provide real mutual exclusion.
    {
        hierarchical_mutex outer(10000), inner(1000);
        long long counter = 0;
        std::atomic<int> refused{0};
        std::atomic<int> concurrent{0};
        std::atomic<int> overlap{0};

        std::vector<std::thread> ts;
        for (int t = 0; t < 8; ++t) {
            ts.emplace_back([&] {
                for (int i = 0; i < 200; ++i) {
                    try {
                        std::lock_guard<hierarchical_mutex> a(outer);
                        std::lock_guard<hierarchical_mutex> b(inner);
                        if (concurrent.fetch_add(1) != 0) overlap.store(1);
                        ++counter;
                        SHAKE();
                        concurrent.fetch_sub(1);
                    } catch (const std::logic_error&) {
                        refused.fetch_add(1);
                    }
                }
            });
        }
        for (auto& t : ts) t.join();

        if (refused.load() != 0) {
            std::printf("%d legal locks were rejected — the current level is shared "
                        "between threads instead of being thread_local\n", refused.load());
            return 1;
        }
        if (overlap.load() != 0) {
            std::printf("two threads were inside the critical section at once — "
                        "the mutex is not actually excluding anyone\n");
            return 1;
        }
        if (counter != 8 * 200) {
            std::printf("counter = %lld, expected %d\n", counter, 8 * 200);
            return 1;
        }
    }

    // 6. Each thread enforces the hierarchy for itself, repeatedly.
    {
        std::atomic<int> missed{0};
        std::vector<std::thread> ts;
        for (int t = 0; t < 4; ++t) {
            ts.emplace_back([&] {
                for (int i = 0; i < 100; ++i) {
                    hierarchical_mutex high(10000), low(1000);
                    low.lock();
                    SHAKE();
                    if (!throws_on_lock(high)) missed.fetch_add(1);
                    low.unlock();
                    // and the correct order still works on this thread afterwards
                    high.lock();
                    low.lock();
                    low.unlock();
                    high.unlock();
                }
            });
        }
        for (auto& t : ts) t.join();
        if (missed.load() != 0) {
            std::printf("%d out-of-order locks slipped through on worker threads\n",
                        missed.load());
            return 1;
        }
    }

    std::printf("hierarchy enforced: descending order allowed, ascending and equal "
                "rejected, state restored, per-thread, still a real mutex\n");
    return 0;
}
