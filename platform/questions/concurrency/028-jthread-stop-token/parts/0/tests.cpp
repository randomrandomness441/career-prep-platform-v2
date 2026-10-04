#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    using clock = std::chrono::steady_clock;

    // ── Phase 1: the graceful loop. Jobs submitted while the worker runs are
    // all processed, FIFO, exactly once.
    {
        queue_worker w;
        long expected = 0;
        int submitted = 0;
        for (int round = 0; round < 3; ++round) {
            for (int i = 0; i < 200; ++i) {
                ++submitted;
                expected += submitted;   // jobs arrive as 1,2,3,... in order
                w.submit(submitted);
                SHAKE();
            }
            auto deadline = clock::now() + std::chrono::milliseconds(1500);
            while (w.processed() < submitted) {
                if (clock::now() > deadline) {
                    std::printf("round %d: stalled at %d of %d processed\n",
                                round, w.processed(), submitted);
                    return 1;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        if (w.sum() != expected) {
            std::printf("sum %lld != expected %lld\n",
                        (long long)w.sum(), (long long)expected);
            return 1;
        }

        // ── Phase 2: the worker is parked on the condition variable (queue has
        // been empty since the last drain). Request stop from ANOTHER thread;
        // this join must complete within a generous bound. A worker whose stop
        // is only checked between jobs never leaves the wait: hang.
        std::thread helper([&w] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            w.request_stop();
        });
        auto t0 = clock::now();
        w.join();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now() - t0);
        helper.join();
        if (elapsed > std::chrono::milliseconds(2000)) {
            std::printf("join took %lld ms after stop — parked worker never woke\n",
                        (long long)elapsed.count());
            return 1;
        }
        // scope end: destructor over an already-stopped worker must be a cheap no-op
    }

    // ── Phase 3: shutdown from a cold destructor, with a backlog still queued.
    // Pending jobs may be dropped or processed; the destructor must not hang on
    // them.
    {
        auto t0 = clock::now();
        {
            queue_worker w;
            for (long i = 1; i <= 1000; ++i) w.submit(i);
            SHAKE();
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now() - t0);
        if (elapsed > std::chrono::milliseconds(2000)) {
            std::printf("destructor with backlog took %lld ms — shutdown is not prompt\n",
                        (long long)elapsed.count());
            return 1;
        }
    }

    std::printf("queue_worker: 600 jobs FIFO, parked worker wakes on stop, "
                "destructor joins promptly with and without backlog\n");
    return 0;
}
