// Harness for "Treiber Stack — Lock-Free push and pop".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

namespace {
const int  kProducers = 4;
const int  kConsumers = 4;
const int  kPerProducer = 4000;
const int  kMixedThreads = 6;
const int  kMixedIters = 3000;
}  // namespace

int main() {
    // ── 1. single-threaded: it is still a stack ──────────────────────────
    {
        LockFreeStack<int> s;
        int v = -1;
        if (s.pop(v)) { std::printf("pop() on an empty stack returned true\n"); return 1; }
        if (!s.empty()) { std::printf("a fresh stack is not empty()\n"); return 1; }

        for (int i = 1; i <= 5; ++i) s.push(i);
        if (s.empty()) { std::printf("stack with 5 elements reports empty()\n"); return 1; }

        for (int expect = 5; expect >= 1; --expect) {
            if (!s.pop(v)) { std::printf("pop() failed with %d elements left\n", expect); return 1; }
            if (v != expect) {
                std::printf("not LIFO: popped %d, expected %d\n", v, expect);
                return 1;
            }
        }
        if (s.pop(v)) { std::printf("popped a 6th element from a 5-element stack\n"); return 1; }
    }

    // ── 2. every pushed element comes out exactly once ───────────────────
    // Four producers push disjoint ranges of integers. Four consumers pop
    // until the expected total has come out. Then we tally: a lost update in
    // push() shows up as a missing value, and a pop() that ignores its
    // compare-exchange result shows up as a duplicate.
    {
        const int total = kProducers * kPerProducer;
        LockFreeStack<int> s;
        std::atomic<long> remaining{total};
        std::vector<std::vector<int>> got(static_cast<size_t>(kConsumers));

        std::vector<std::thread> ts;
        for (int p = 0; p < kProducers; ++p) {
            ts.emplace_back([&s, p] {
                SHAKE();
                for (int i = 0; i < kPerProducer; ++i) {
                    s.push(p * kPerProducer + i);
                    SHAKE();
                }
            });
        }
        for (int c = 0; c < kConsumers; ++c) {
            ts.emplace_back([&s, &remaining, &got, c] {
                std::vector<int>& mine = got[static_cast<size_t>(c)];
                mine.reserve(static_cast<size_t>(kPerProducer));
                SHAKE();
                while (remaining.load(std::memory_order_relaxed) > 0) {
                    int v = 0;
                    if (s.pop(v)) {
                        mine.push_back(v);
                        remaining.fetch_sub(1, std::memory_order_relaxed);
                    } else {
                        std::this_thread::yield();
                    }
                    SHAKE();
                }
            });
        }
        for (std::thread& th : ts) th.join();

        std::vector<int> seen(static_cast<size_t>(total), 0);
        long popped = 0;
        for (const std::vector<int>& mine : got) {
            for (int v : mine) {
                ++popped;
                if (v < 0 || v >= total) {
                    std::printf("popped %d, which was never pushed (valid range 0..%d) — "
                                "the stack handed out a corrupted or freed node\n", v, total - 1);
                    return 1;
                }
                if (++seen[static_cast<size_t>(v)] > 1) {
                    std::printf("value %d came out of the stack twice — two threads popped the "
                                "same node, so a compare-exchange result was ignored\n", v);
                    return 1;
                }
            }
        }
        if (popped != total) {
            std::printf("pushed %d values but popped %ld\n", total, popped);
            return 1;
        }
        for (int i = 0; i < total; ++i) {
            if (seen[static_cast<size_t>(i)] == 0) {
                std::printf("value %d was pushed but never came out — a push overwrote another "
                            "push's node\n", i);
                return 1;
            }
        }
        int leftover = 0;
        if (s.pop(leftover)) {
            std::printf("all %d values were accounted for, yet the stack still holds %d\n",
                        total, leftover);
            return 1;
        }
    }

    // ── 3. every thread pushes and pops, nothing is created or destroyed ──
    // This is the shape that exercises ABA: nodes are popped and new nodes are
    // pushed constantly, so an allocator that hands the same address back can
    // make a stale compare-exchange succeed. If it does, the linked list is
    // spliced wrongly and the totals stop matching.
    {
        LockFreeStack<long> s;
        std::vector<long> pushed_sum(static_cast<size_t>(kMixedThreads), 0);
        std::vector<long> popped_sum(static_cast<size_t>(kMixedThreads), 0);

        std::vector<std::thread> ts;
        for (int t = 0; t < kMixedThreads; ++t) {
            ts.emplace_back([&s, &pushed_sum, &popped_sum, t] {
                const size_t idx = static_cast<size_t>(t);
                SHAKE();
                for (int i = 0; i < kMixedIters; ++i) {
                    const long value = static_cast<long>(t) * kMixedIters + i + 1;
                    pushed_sum[idx] += value;
                    s.push(value);
                    long v = 0;
                    if (s.pop(v)) popped_sum[idx] += v;
                    SHAKE();
                }
            });
        }
        for (std::thread& th : ts) th.join();

        long in = 0, out = 0;
        for (int t = 0; t < kMixedThreads; ++t) {
            in  += pushed_sum[static_cast<size_t>(t)];
            out += popped_sum[static_cast<size_t>(t)];
        }
        long left = 0, v = 0;
        long left_count = 0;
        while (s.pop(v)) { left += v; ++left_count; }
        if (in != out + left) {
            std::printf("values went missing or were duplicated: pushed a total of %ld, "
                        "popped %ld, %ld still on the stack (%ld nodes) — sum is off by %ld\n",
                        in, out, left, left_count, in - (out + left));
            return 1;
        }
    }

    std::printf("%d producers x %d values popped exactly once by %d consumers; "
                "%d threads x %d push/pop pairs conserved every value\n",
                kProducers, kPerProducer, kConsumers, kMixedThreads, kMixedIters);
    return 0;
}
