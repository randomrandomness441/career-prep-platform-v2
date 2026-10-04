// Harness for "Backpressure".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <optional>
#include <thread>
#include <vector>

namespace {

using ms = std::chrono::milliseconds;

const std::size_t kCap = 4;

}  // namespace

int main() {
    // ── 1. THE BOUND. DropNewest, no consumer at all, twice the capacity in. ──
    //    An unbounded queue holds all 10. A bounded one holds 4 and says so.
    {
        backpressure_queue<int> q(kCap, overflow_policy::DropNewest);
        int accepted = 0;
        for (int i = 0; i < 10; ++i) if (q.push(i)) ++accepted;

        if (q.size() > q.capacity()) {
            std::printf("pushed 10 items into a queue of capacity %zu and it is holding "
                        "%zu of them.\nThat is an unbounded queue: nothing here limits "
                        "memory, and a producer that outruns its consumer will grow it "
                        "until the process is killed.\n",
                        q.capacity(), q.size());
            return 1;
        }
        if (q.size() != kCap) {
            std::printf("DropNewest: expected the queue to be full (%zu), it holds %zu\n",
                        kCap, q.size());
            return 1;
        }
        if (accepted != static_cast<int>(kCap)) {
            std::printf("DropNewest: push() reported success %d times, expected %zu. "
                        "A refused item must return false.\n", accepted, kCap);
            return 1;
        }
        if (q.dropped() != 10 - kCap) {
            std::printf("DropNewest: dropped() = %zu, expected %zu. Every discarded item "
                        "must be counted -- an uncounted drop is silent data loss.\n",
                        q.dropped(), 10 - kCap);
            return 1;
        }
        // DropNewest keeps the BACKLOG and refuses the arrivals.
        for (int i = 0; i < static_cast<int>(kCap); ++i) {
            std::optional<int> v = q.try_pop();
            if (!v || *v != i) {
                std::printf("DropNewest kept the wrong items: expected %d at position %d\n",
                            i, i);
                return 1;
            }
        }
    }

    // ── 2. DropOldest keeps the ARRIVALS and evicts the backlog ──────────────
    {
        backpressure_queue<int> q(kCap, overflow_policy::DropOldest);
        for (int i = 0; i < 10; ++i) {
            if (!q.push(i)) { std::printf("DropOldest: push() must accept the new item\n"); return 1; }
        }
        if (q.size() != kCap) {
            std::printf("DropOldest: queue holds %zu, capacity is %zu\n", q.size(), kCap);
            return 1;
        }
        if (q.dropped() != 10 - kCap) {
            std::printf("DropOldest: dropped() = %zu, expected %zu\n", q.dropped(), 10 - kCap);
            return 1;
        }
        for (int i = 6; i < 10; ++i) {
            std::optional<int> v = q.try_pop();
            if (!v || *v != i) {
                std::printf("DropOldest kept the wrong items: expected %d, got %s\n",
                            i, v ? "something else" : "nothing");
                return 1;
            }
        }
    }

    // ── 3. Block: the producer stops. That IS the signal. ────────────────────
    {
        backpressure_queue<int> q(kCap, overflow_policy::Block);
        for (std::size_t i = 0; i < kCap; ++i) q.push(static_cast<int>(i));

        std::atomic<bool> returned{false};
        std::thread producer([&] { q.push(99); returned.store(true); });

        std::this_thread::sleep_for(ms(40));
        if (returned.load()) {
            producer.join();
            std::printf("Block policy: push() on a full queue returned instead of "
                        "blocking. The producer was never told to slow down, so it "
                        "will keep running at full speed.\n");
            return 1;
        }
        std::optional<int> v = q.try_pop();     // make room
        producer.join();
        if (!v || *v != 0) { std::printf("Block: try_pop gave the wrong item\n"); return 1; }
        if (!returned.load()) { std::printf("Block: making room did not release the producer\n"); return 1; }
        if (q.dropped() != 0) {
            std::printf("Block policy dropped %zu items. Blocking must never lose data.\n",
                        q.dropped());
            return 1;
        }
    }

    // ── 4. A producer that outruns its consumer, for real. ───────────────────
    //    The consumer is deliberately slower. Under Block the queue must stay
    //    inside its bound the whole way, and not a single item may be lost.
    {
        const int kItems = 3000;
        backpressure_queue<int> q(16, overflow_policy::Block);
        std::atomic<std::size_t> peak{0};
        std::atomic<bool> watching{true};

        std::thread monitor([&] {
            while (watching.load()) {
                std::size_t s = q.size();
                std::size_t p = peak.load();
                while (s > p && !peak.compare_exchange_weak(p, s)) { }
                std::this_thread::yield();
            }
        });

        std::atomic<int> received{0};
        std::atomic<bool> order_ok{true};
        std::thread consumer([&] {
            int expect = 0;
            for (;;) {
                std::optional<int> v = q.pop();
                if (!v) return;
                if (*v != expect++) order_ok.store(false);
                received.fetch_add(1);
                SHAKE();                     // the slow side
            }
        });

        for (int i = 0; i < kItems; ++i) q.push(i);   // as fast as it can go
        q.close();
        consumer.join();
        watching.store(false);
        monitor.join();

        if (peak.load() > q.capacity()) {
            std::printf("a fast producer drove the queue to %zu items against a capacity "
                        "of %zu. Memory use is set by the producer's speed, not by any "
                        "limit you chose.\n", peak.load(), q.capacity());
            return 1;
        }
        if (received.load() != kItems || !order_ok.load()) {
            std::printf("Block policy: %d of %d items arrived, in order: %s\n",
                        received.load(), kItems, order_ok.load() ? "yes" : "no");
            return 1;
        }
        if (q.dropped() != 0) {
            std::printf("Block policy dropped %zu items under load\n", q.dropped());
            return 1;
        }
    }

    // ── 5. same pressure, drop policies: bounded memory, counted losses ──────
    {
        for (overflow_policy p : {overflow_policy::DropNewest, overflow_policy::DropOldest}) {
            backpressure_queue<int> q(8, p);
            std::atomic<int> received{0};
            std::atomic<bool> go{false};
            std::thread consumer([&] {
                while (!go.load()) std::this_thread::yield();
                for (;;) {
                    std::optional<int> v = q.pop();
                    if (!v) return;
                    received.fetch_add(1);
                    SHAKE();
                }
            });

            const int kItems = 2000;
            for (int i = 0; i < kItems; ++i) {
                q.push(i);
                if (q.size() > q.capacity()) {
                    std::printf("drop policy: queue grew to %zu past a capacity of %zu\n",
                                q.size(), q.capacity());
                    go.store(true); q.close(); consumer.join();
                    return 1;
                }
            }
            go.store(true);
            q.close();
            consumer.join();

            // Nothing may vanish without being counted.
            if (received.load() + static_cast<int>(q.dropped()) != kItems) {
                std::printf("drop policy: %d items in, %d received + %zu counted as "
                            "dropped = %d. The missing ones were lost silently.\n",
                            kItems, received.load(), q.dropped(),
                            received.load() + static_cast<int>(q.dropped()));
                return 1;
            }
            if (q.dropped() == 0) {
                std::printf("drop policy: a consumer that never ran, %d items pushed into "
                            "a queue of 8, and nothing was dropped. Where did they go?\n",
                            kItems);
                return 1;
            }
        }
    }

    std::printf("bounded under a fast producer, all three overflow policies correct, "
                "every dropped item counted\n");
    return 0;
}
