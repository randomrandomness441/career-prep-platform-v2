#include <atomic>

class HotCounter {
    std::atomic<long> counter_{0};

public:
    HotCounter() noexcept = default;

    // One variable, one operation, nothing else to keep in sync with it --
    // the hardware's atomic read-modify-write instruction does the whole
    // job in one indivisible step. No lock, no syscall, no thread ever
    // sleeps for this.
    void increment() noexcept { counter_.fetch_add(1, std::memory_order_relaxed); }

    long get() const noexcept { return counter_.load(std::memory_order_relaxed); }
};
