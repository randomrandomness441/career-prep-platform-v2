#include <atomic>

// A minimal spinlock: lock() busy-waits until it can atomically flip the
// flag from unlocked to locked; unlock() flips it back.
class Spinlock {
public:
    void lock() {
        // exchange(true) atomically sets the flag AND tells us what it was
        // a moment ago, in one indivisible step. While it keeps coming back
        // "already true," someone else holds the lock -- keep trying. The
        // instant it comes back false, we just flipped it ourselves, and no
        // other thread can have done the same flip at the same moment.
        while (locked_.exchange(true, std::memory_order_acquire)) {
            // spin
        }
    }

    void unlock() { locked_.store(false, std::memory_order_release); }

private:
    std::atomic<bool> locked_{false};
};
