#include <atomic>

// Two threads each "arrive", check whether the other has arrived yet, and
// return what they saw. The invariant: it must never be possible for BOTH
// arriveA() and arriveB() to come back 0 in the same run, meaning neither
// thread saw the other having arrived, even though both of them definitely
// did.
class Handshake {
    std::atomic<int> a_{0};
    std::atomic<int> b_{0};

public:
    int arriveA() {
        // TODO: pick a memory order for each operation in this class.
        a_.store(1, std::memory_order_relaxed);
        return b_.load(std::memory_order_relaxed);
    }

    int arriveB() {
        b_.store(1, std::memory_order_relaxed);
        return a_.load(std::memory_order_relaxed);
    }
};
