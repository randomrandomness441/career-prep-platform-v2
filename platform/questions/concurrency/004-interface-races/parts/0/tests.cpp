// Harness for "Race Conditions Inherent in Interfaces".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <exception>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {
const int kItems   = 3000;
const int kThreads = 8;
const int kTrials  = 15;
}  // namespace

int main() {
    // ── 1. single-threaded sanity ────────────────────────────────────────
    {
        threadsafe_stack s;
        if (!s.empty()) {
            std::printf("a freshly constructed stack reports itself non-empty\n");
            return 1;
        }
        if (s.pop().has_value()) {
            std::printf("pop() on an empty stack returned a value\n");
            return 1;
        }
        s.push(1);
        s.push(2);
        std::optional<int> a = s.pop();
        std::optional<int> b = s.pop();
        if (!a || !b || *a != 2 || *b != 1) {
            std::printf("LIFO order broken: got %d then %d, expected 2 then 1\n",
                        a ? *a : -1, b ? *b : -1);
            return 1;
        }
        if (s.pop().has_value()) {
            std::printf("stack should be empty after popping everything\n");
            return 1;
        }
    }

    // ── 2. concurrent drain ──────────────────────────────────────────────
    // kItems distinct values go in. kThreads threads race to drain the stack.
    // Whatever the schedule, every value must come out of exactly one pop().
    // Handing the same value to two threads, or losing one, is the bug.
    for (int trial = 0; trial < kTrials; ++trial) {
        threadsafe_stack s;
        for (int i = 0; i < kItems; ++i) s.push(i);

        std::vector<std::vector<int>> got(kThreads);
        std::atomic<bool> threw{false};
        std::mutex msg_m;
        std::string msg;

        std::vector<std::thread> ts;
        for (int t = 0; t < kThreads; ++t) {
            ts.emplace_back([&, t] {
                SHAKE();
                try {
                    for (;;) {
                        std::optional<int> v = s.pop();
                        if (!v) break;
                        got[t].push_back(*v);
                    }
                } catch (const std::exception& e) {
                    threw.store(true);
                    std::lock_guard<std::mutex> g(msg_m);
                    if (msg.empty()) msg = e.what();
                }
                SHAKE();
            });
        }
        for (std::thread& th : ts) th.join();

        if (threw.load()) {
            std::printf("trial %d: pop() threw \"%s\"\n"
                        "the stack was drained by another thread between the emptiness "
                        "check and the read\n", trial, msg.c_str());
            return 1;
        }

        std::vector<int> seen(kItems, 0);
        long total = 0;
        for (const std::vector<int>& v : got) {
            for (int x : v) {
                ++total;
                if (x < 0 || x >= kItems) {
                    std::printf("trial %d: pop() returned %d, which was never pushed\n",
                                trial, x);
                    return 1;
                }
                ++seen[x];
            }
        }

        int dup = -1, missing = -1, dup_count = 0, missing_count = 0;
        for (int i = 0; i < kItems; ++i) {
            if (seen[i] > 1) { ++dup_count; if (dup < 0) dup = i; }
            if (seen[i] == 0) { ++missing_count; if (missing < 0) missing = i; }
        }
        if (dup_count || missing_count || total != kItems) {
            std::printf("trial %d: %d values were popped twice, %d were lost "
                        "(%ld pops returned a value, %d were pushed)\n",
                        trial, dup_count, missing_count, total, kItems);
            if (dup >= 0)
                std::printf("  value %d was handed to two different threads\n", dup);
            if (missing >= 0)
                std::printf("  value %d was removed from the stack but never returned\n",
                            missing);
            std::printf("pop() is not atomic: another thread changed the stack between "
                        "the check and the removal\n");
            return 1;
        }
    }

    std::printf("%d trials, %d threads draining %d items: every value popped exactly once\n",
                kTrials, kThreads, kItems);
    return 0;
}
