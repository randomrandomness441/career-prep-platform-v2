// Harness for "A Bounded Blocking Queue".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <optional>
#include <thread>
#include <vector>

namespace {

using clk = std::chrono::steady_clock;
using ms  = std::chrono::milliseconds;

// Small capacity plus many threads on both sides is the shape that exposes a
// shared condition variable. With capacity 2 the queue is constantly flipping
// between "full" and "empty", so producers and consumers are parked at the
// same time for most of the run.
const int kCapacity  = 2;
const int kProducers = 4;
const int kConsumers = 4;
const int kPerProd   = 200;
const int kTotal     = kProducers * kPerProd;

}  // namespace

int main() {
    // 1. single threaded: FIFO order, and the bound is a bound
    {
        bounded_queue q(3);
        if (q.capacity() != 3u) { std::printf("capacity() wrong\n"); return 1; }
        for (int i = 0; i < 3; ++i) {
            if (!q.try_enqueue(i)) { std::printf("try_enqueue failed with room left\n"); return 1; }
        }
        if (q.size() != 3u) { std::printf("size() = %zu, expected 3\n", q.size()); return 1; }
        if (q.try_enqueue(99)) {
            std::printf("try_enqueue succeeded past the capacity: size is now %zu, "
                        "capacity is %zu\n", q.size(), q.capacity());
            return 1;
        }
        for (int i = 0; i < 3; ++i) {
            std::optional<int> v = q.try_dequeue();
            if (!v || *v != i) { std::printf("queue is not FIFO\n"); return 1; }
        }
        if (q.try_dequeue()) { std::printf("try_dequeue returned a value from an empty queue\n"); return 1; }
    }

    // 2. enqueue must actually BLOCK when full, and dequeue when empty
    {
        bounded_queue q(1);
        q.enqueue(1);                       // now full
        std::atomic<bool> got_in{false};
        std::thread blocked([&] {
            q.enqueue(2);                   // must park until we make room
            got_in.store(true);
        });
        std::this_thread::sleep_for(ms(40));
        if (got_in.load()) {
            std::printf("enqueue on a full queue returned instead of blocking\n");
            blocked.join();
            return 1;
        }
        std::optional<int> first = q.dequeue();
        if (!first || *first != 1) { std::printf("dequeue returned the wrong element\n"); return 1; }
        blocked.join();
        if (!got_in.load()) { std::printf("making room did not wake the blocked producer\n"); return 1; }

        // and the mirror image
        bounded_queue e(4);
        std::atomic<bool> got_out{false};
        std::thread waiter([&] {
            std::optional<int> v = e.dequeue();
            if (v && *v == 7) got_out.store(true);
        });
        std::this_thread::sleep_for(ms(40));
        if (got_out.load()) { std::printf("dequeue on an empty queue returned a value\n"); return 1; }
        e.enqueue(7);
        waiter.join();
        if (!got_out.load()) { std::printf("an arriving item did not wake the blocked consumer\n"); return 1; }
    }

    // 3. THE POINT: many producers and many consumers on a small queue.
    //    Every item must come out exactly once, and the run must finish.
    {
        bounded_queue q(static_cast<std::size_t>(kCapacity));
        std::atomic<int> received{0};
        std::atomic<long long> sum{0};
        std::vector<std::atomic<int>> seen(kTotal);
        for (std::atomic<int>& s : seen) s.store(0);

        std::vector<std::thread> consumers;
        for (int c = 0; c < kConsumers; ++c) {
            consumers.emplace_back([&] {
                for (;;) {
                    std::optional<int> v = q.dequeue();
                    if (!v) return;                  // closed and drained
                    seen[static_cast<std::size_t>(*v)].fetch_add(1);
                    sum.fetch_add(*v);
                    received.fetch_add(1);
                    SHAKE();
                }
            });
        }

        std::vector<std::thread> producers;
        for (int p = 0; p < kProducers; ++p) {
            producers.emplace_back([&, p] {
                for (int i = 0; i < kPerProd; ++i) {
                    SHAKE();
                    q.enqueue(p * kPerProd + i);
                }
            });
        }

        for (std::thread& t : producers) t.join();
        q.close();
        for (std::thread& t : consumers) t.join();

        long long expected = 0;
        for (int i = 0; i < kTotal; ++i) expected += i;
        if (received.load() != kTotal || sum.load() != expected) {
            std::printf("%d producers x %d items through a queue of capacity %d: "
                        "received %d of %d, checksum %lld vs %lld\n",
                        kProducers, kPerProd, kCapacity,
                        received.load(), kTotal, sum.load(), expected);
            return 1;
        }
        for (int i = 0; i < kTotal; ++i) {
            if (seen[static_cast<std::size_t>(i)].load() != 1) {
                std::printf("item %d came out %d times, expected exactly 1\n",
                            i, seen[static_cast<std::size_t>(i)].load());
                return 1;
            }
        }
        if (q.size() != 0u) { std::printf("queue not empty at the end: %zu left\n", q.size()); return 1; }
    }

    // 4. close() must release threads that are already parked in wait()
    {
        bounded_queue q(1);
        std::atomic<int> woke{0};
        std::vector<std::thread> waiters;
        for (int i = 0; i < 6; ++i) {
            waiters.emplace_back([&] {
                std::optional<int> v = q.dequeue();       // parks: queue is empty
                if (!v) woke.fetch_add(1);
            });
        }
        // a producer parked on the other condition, too
        q.enqueue(1);                                     // full
        std::atomic<int> refused{0};
        std::vector<std::thread> pushers;
        for (int i = 0; i < 3; ++i) {
            pushers.emplace_back([&] {
                if (!q.enqueue(2)) refused.fetch_add(1);  // parks: queue is full
            });
        }

        std::this_thread::sleep_for(ms(30));
        SHAKE();
        q.close();
        for (std::thread& t : waiters) t.join();
        for (std::thread& t : pushers) t.join();

        // The one item that was in the queue goes to one of the six waiters;
        // whoever pushed before the close may also have landed an item. So at
        // least 6 - (1 + 3) of them must have seen the end-of-stream, and every
        // pusher still parked at close() time must have been refused.
        if (woke.load() + refused.load() == 0) {
            std::printf("close() woke nobody: %d consumers saw end-of-stream, "
                        "%d producers were refused\n", woke.load(), refused.load());
            return 1;
        }
        if (!q.is_closed()) { std::printf("is_closed() false after close()\n"); return 1; }
        if (q.dequeue()) { std::printf("dequeue on a closed, drained queue returned a value\n"); return 1; }
    }

    // 5. a closed queue still hands out what is already in it
    {
        bounded_queue q(8);
        for (int i = 0; i < 5; ++i) q.enqueue(i);
        q.close();
        for (int i = 0; i < 5; ++i) {
            std::optional<int> v = q.dequeue();
            if (!v || *v != i) {
                std::printf("close() dropped buffered items: expected %d, got %s\n",
                            i, v ? "a different value" : "end-of-stream");
                return 1;
            }
        }
        if (q.dequeue()) { std::printf("drained closed queue still returns values\n"); return 1; }
        if (q.enqueue(1)) { std::printf("enqueue on a closed queue reported success\n"); return 1; }
    }

    std::printf("bounded, blocking on both sides, %d items through capacity %d with "
                "%d producers and %d consumers, clean shutdown\n",
                kTotal, kCapacity, kProducers, kConsumers);
    return 0;
}
