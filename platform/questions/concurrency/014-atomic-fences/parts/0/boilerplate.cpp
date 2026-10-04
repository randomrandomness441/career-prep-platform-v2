#include <atomic>

// Same handshake as the previous question, same invariant: arriveA() and
// arriveB() must never both come back 0 in the same run. This time, fix it
// with std::atomic_thread_fence rather than by naming a stronger memory
// order on the atomics themselves.
class Handshake {
    std::atomic<int> a_{0};
    std::atomic<int> b_{0};

public:
    int arriveA() {
        a_.store(1, std::memory_order_relaxed);
        // TODO: where does a fence need to go, on each side, to fix this?
        return b_.load(std::memory_order_relaxed);
    }

    int arriveB() {
        b_.store(1, std::memory_order_relaxed);
        return a_.load(std::memory_order_relaxed);
    }
};
