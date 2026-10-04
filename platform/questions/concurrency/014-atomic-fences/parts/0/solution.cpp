#include <atomic>

// Same two relaxed flags as the naive version. The fix is a fence between
// the store and the load, on both sides, instead of naming a stronger order
// on the atomics themselves.
//
// A seq_cst fence participates in the same single global order a seq_cst
// atomic operation does. Placed here, it gives "store, then fence, then
// load" the ordering guarantee 013 got by tagging every operation seq_cst
// directly -- but the fence's effect isn't tied to a_ or b_ specifically,
// it covers every atomic operation around it on this thread. That's the
// point: one fence can guard many relaxed operations at once, which is
// cheaper to reason about (and, with more operations, cheaper to run) than
// upgrading every one of them individually. See reading section 4.
class Handshake {
    std::atomic<int> a_{0};
    std::atomic<int> b_{0};

public:
    int arriveA() {
        a_.store(1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        return b_.load(std::memory_order_relaxed);
    }

    int arriveB() {
        b_.store(1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        return a_.load(std::memory_order_relaxed);
    }
};
