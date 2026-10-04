// Harness for "Multithreaded Memory Pool".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace {

// Written into a block right after allocate() and checked right before
// deallocate(). If the pool ever hands the same block to two live owners at
// once, one thread's write stomps the other's, and this mismatches.
struct Tag {
    int tid;
    long counter;
    unsigned long long check;
};

}  // namespace

int main() {
    // ── 1. single-threaded: capacity, exhaustion, and refill ─────────────
    {
        MemoryPool<64, 4> pool;
        void* got[4] = {};
        for (int i = 0; i < 4; ++i) {
            got[i] = pool.allocate();
            if (!got[i]) { std::printf("allocate %d of 4 failed on a fresh pool\n", i); return 1; }
        }
        if (pool.allocate() != nullptr) {
            std::printf("allocate succeeded on an exhausted pool\n");
            return 1;
        }
        for (int i = 0; i < 4; ++i) {
            for (int j = i + 1; j < 4; ++j) {
                if (got[i] == got[j]) { std::printf("two live allocations aliased the same block\n"); return 1; }
            }
        }
        for (int i = 0; i < 4; ++i) pool.deallocate(got[i]);
        if (pool.blocks_in_use() != 0) {
            std::printf("single-threaded: blocks_in_use() = %zu after freeing everything, expected 0\n",
                        pool.blocks_in_use());
            return 1;
        }
        void* p = pool.allocate();
        if (!p) { std::printf("allocate failed after a full free/refill cycle\n"); return 1; }
        pool.deallocate(p);
    }

    // ── 2. double free must not corrupt the free list ─────────────────────
    // A second deallocate() of the same pointer must be refused, not silently
    // pushed onto the free list a second time -- otherwise two later
    // allocate() calls would hand out the SAME block to two different
    // callers, which is exactly the bug this pool's API must not allow.
    {
        MemoryPool<64, 8> pool;
        void* p = pool.allocate();
        if (!p) { std::printf("allocate failed on a fresh 8-block pool\n"); return 1; }
        if (!pool.deallocate(p)) { std::printf("first deallocate() was refused\n"); return 1; }
        if (pool.deallocate(p)) { std::printf("a double free was accepted instead of refused\n"); return 1; }

        // Drain the whole pool and confirm every address handed out is
        // unique -- if the double free had corrupted the list, some address
        // would come out twice here.
        void* seen[8] = {};
        int n = 0;
        for (void* q; (q = pool.allocate()) != nullptr; ) {
            for (int i = 0; i < n; ++i) {
                if (seen[i] == q) {
                    std::printf("block %p came out of allocate() twice after a double free\n", q);
                    return 1;
                }
            }
            seen[n++] = q;
        }
        if (n != 8) {
            std::printf("drained %d blocks from an 8-block pool after a double free, expected 8\n", n);
            return 1;
        }
    }

    // ── 3. concurrent allocate/write/verify/deallocate, no aliasing ──────
    // A small pool relative to the thread count forces heavy recycling of
    // the same physical blocks -- exactly the condition that widens the ABA
    // window in a Treiber-style free list that doesn't tag its head.
    {
        constexpr int kThreads = 8;
        constexpr int kIters = 6000;
        MemoryPool<64, 37> pool;
        std::atomic<bool> corrupted{false};
        std::atomic<long> completed{0};

        std::vector<std::thread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&, t] {
                for (long i = 0; i < kIters; ++i) {
                    void* p;
                    while ((p = pool.allocate()) == nullptr) { SHAKE(); }
                    Tag tag{t, i, static_cast<unsigned long long>(t) * 1000003ull + static_cast<unsigned long long>(i)};
                    std::memcpy(p, &tag, sizeof(tag));
                    if ((i & 511) == 0) SHAKE();
                    Tag back;
                    std::memcpy(&back, p, sizeof(back));
                    if (back.tid != tag.tid || back.counter != tag.counter || back.check != tag.check) {
                        corrupted.store(true, std::memory_order_relaxed);
                    }
                    pool.deallocate(p);
                    completed.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& th : threads) th.join();

        if (corrupted.load()) {
            std::printf("a block's contents changed between write and read-back while it was "
                        "supposedly owned by one thread -- two threads held the same block\n");
            return 1;
        }
        if (completed.load() != static_cast<long>(kThreads) * kIters) {
            std::printf("%ld allocate/deallocate cycles completed, expected %ld\n",
                        completed.load(), static_cast<long>(kThreads) * kIters);
            return 1;
        }
        // Every thread balanced its own allocate()s with a deallocate(): the
        // true in-use count must be exactly 0. A counter updated outside the
        // pool's lock loses updates under this much contention and will not
        // land on exactly 0.
        if (pool.blocks_in_use() != 0) {
            std::printf("blocks_in_use() = %zu after %d threads x %d balanced alloc/dealloc "
                        "cycles, expected 0 -- the in-use counter lost updates under contention\n",
                        pool.blocks_in_use(), kThreads, kIters);
            return 1;
        }
    }

    std::printf("capacity/exhaustion, double-free rejection, and %d threads x %d concurrent "
                "alloc/write/verify/dealloc cycles: no aliasing, blocks_in_use() exact\n", 8, 6000);
    return 0;
}
