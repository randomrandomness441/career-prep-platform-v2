// Harness for "SPSC Ring Buffer".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <thread>

namespace {

// A two-word payload with a redundant field. If the consumer is allowed to see
// a published index before the element write that preceded it, it reads a slot
// holding an older item — or, on the very first lap, a zeroed one — and `check`
// stops matching `seq`.
struct Item {
    long seq = 0;
    long check = 0;
};
Item make_item(long i) { return Item{i, ~i}; }

const int  kTrials = 12;
const long kItems  = 20000;

}  // namespace

int main() {
    // ── 1. single-threaded: it is a FIFO queue with a fixed capacity ─────
    {
        SpscRing<int, 8> r;
        int v = -1;
        if (r.pop(v)) { std::printf("pop() on an empty ring returned true\n"); return 1; }

        const std::size_t cap = r.capacity();
        for (std::size_t i = 0; i < cap; ++i) {
            if (!r.push(static_cast<int>(i))) {
                std::printf("push %zu of %zu failed on an empty ring\n", i + 1, cap);
                return 1;
            }
        }
        if (r.push(999)) {
            std::printf("pushed %zu items into a ring of capacity %zu\n", cap + 1, cap);
            return 1;
        }
        for (std::size_t i = 0; i < cap; ++i) {
            if (!r.pop(v)) { std::printf("pop %zu of %zu failed on a full ring\n", i + 1, cap); return 1; }
            if (v != static_cast<int>(i)) {
                std::printf("not FIFO: popped %d, expected %zu\n", v, i);
                return 1;
            }
        }
        if (r.pop(v)) { std::printf("popped one item too many\n"); return 1; }
    }

    // ── 2. wrap-around: the indices must keep working past the end ───────
    {
        SpscRing<long, 4> r;
        long v = 0;
        for (long i = 0; i < 1000; ++i) {
            if (!r.push(i)) { std::printf("push failed at i=%ld with an empty ring\n", i); return 1; }
            if (!r.pop(v))  { std::printf("pop failed at i=%ld with one item queued\n", i); return 1; }
            if (v != i)     { std::printf("wrap-around lost order: got %ld, expected %ld\n", v, i); return 1; }
        }
        // Half-fill, drain, repeat — exercises the wrap in the middle of a lap.
        for (int lap = 0; lap < 200; ++lap) {
            if (!r.push(1) || !r.push(2)) { std::printf("lap %d: push failed\n", lap); return 1; }
            long a = 0, b = 0;
            if (!r.pop(a) || !r.pop(b)) { std::printf("lap %d: pop failed\n", lap); return 1; }
            if (a != 1 || b != 2) { std::printf("lap %d: got %ld,%ld expected 1,2\n", lap, a, b); return 1; }
        }
    }

    // ── 3. one producer, one consumer, many trials ───────────────────────
    // The producer pushes 0,1,2,... in order. The consumer must see exactly
    // that sequence, with every item internally consistent.
    //
    // A relaxed store to the head index publishes the index without publishing
    // the element, so the consumer can be told slot h is full and still read
    // whatever was in it before. A ring big enough that the producer never
    // blocks keeps the consumer reading slots the producer has only just
    // touched, which is where the window is widest.
    for (int trial = 0; trial < kTrials; ++trial) {
        SpscRing<Item, 1024> r;
        long bad_seq = -1, bad_check_at = -1;
        long got_seq = 0, got_check = 0;

        std::thread producer([&r] {
            SHAKE();
            for (long i = 0; i < kItems; ++i) {
                const Item it = make_item(i);
                while (!r.push(it)) { /* full: spin, the consumer will drain it */ }
                if ((i & 1023) == 0) SHAKE();
            }
        });

        std::thread consumer([&] {
            SHAKE();
            for (long i = 0; i < kItems; ++i) {
                Item it;
                while (!r.pop(it)) { /* empty: spin, the producer will fill it */ }
                if (it.check != ~it.seq && bad_check_at < 0) {
                    bad_check_at = i; got_seq = it.seq; got_check = it.check;
                }
                if (it.seq != i && bad_seq < 0) {
                    bad_seq = i; got_seq = it.seq;
                }
                if ((i & 1023) == 0) SHAKE();
            }
        });

        producer.join();
        consumer.join();

        if (bad_check_at >= 0) {
            std::printf("trial %d: item %ld came out inconsistent — seq=%ld but check=%ld "
                        "(expected %ld). The consumer saw a published index before the "
                        "element write that produced it.\n",
                        trial, bad_check_at, got_seq, got_check, ~got_seq);
            return 1;
        }
        if (bad_seq >= 0) {
            std::printf("trial %d: expected item %ld, got %ld — the sequence was not "
                        "delivered in order, or a slot was read before it was written.\n",
                        trial, bad_seq, got_seq);
            return 1;
        }
        Item leftover;
        if (r.pop(leftover)) {
            std::printf("trial %d: %ld items pushed and popped, but the ring still holds "
                        "seq=%ld\n", trial, kItems, leftover.seq);
            return 1;
        }
    }

    // ── 4. a ring of the minimum size, where full and empty alternate ────
    // Capacity 2 means one usable slot, so every push must wait for the
    // previous pop. This is the tightest possible handoff.
    {
        SpscRing<Item, 2> r;
        constexpr long n = 20000;
        long mismatch = -1, seen_seq = 0;

        std::thread producer([&r] {
            for (long i = 0; i < n; ++i) { const Item it = make_item(i);
                                           while (!r.push(it)) {} }
        });
        std::thread consumer([&] {
            for (long i = 0; i < n; ++i) {
                Item it;
                while (!r.pop(it)) {}
                if ((it.seq != i || it.check != ~it.seq) && mismatch < 0) {
                    mismatch = i; seen_seq = it.seq;
                }
            }
        });
        producer.join();
        consumer.join();
        if (mismatch >= 0) {
            std::printf("single-slot ring: item %ld came out as seq=%ld — the handoff is "
                        "not ordered\n", mismatch, seen_seq);
            return 1;
        }
    }

    std::printf("%d trials of %ld items through a 1024-slot ring, plus %d through a "
                "single-slot ring: every item in order and internally consistent\n",
                kTrials, kItems, 20000);
    return 0;
}
