#include <atomic>

// Same two flags, same two operations. The only change from the naive
// version is the memory order: seq_cst instead of relaxed (written out
// explicitly here; leaving the order unnamed defaults to the same thing).
//
// seq_cst is the one memory order that places every seq_cst operation, across
// every thread, into a single global order that all threads agree on. That
// is a strictly stronger guarantee than release/acquire, which only orders
// one specific store against the one load that reads it -- see reading
// section 2 for why release/acquire alone does not fix this particular
// pattern.
class Handshake {
    std::atomic<int> a_{0};
    std::atomic<int> b_{0};

public:
    int arriveA() {
        a_.store(1, std::memory_order_seq_cst);
        return b_.load(std::memory_order_seq_cst);
    }

    int arriveB() {
        b_.store(1, std::memory_order_seq_cst);
        return a_.load(std::memory_order_seq_cst);
    }
};
