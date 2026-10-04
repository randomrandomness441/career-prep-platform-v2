#include <atomic>
#include <condition_variable>
#include <mutex>

// A bounded resource pool — think a fixed-size connection pool. Same shape as
// the naive version, one change: notify_one instead of notify_all.
//
// That's safe here specifically because every waiter shares the exact same
// predicate (free_ > 0) -- there is no "which waiter" to target, any one of
// them can take the slot that just opened. See reading section 1/2.
class SlotPool {
public:
    explicit SlotPool(int slots) : free_(slots) {}

    std::atomic<long> wasted_wakeups{0};
    std::atomic<long> started{0};

    void acquire() {
        started.fetch_add(1, std::memory_order_relaxed);
        std::unique_lock<std::mutex> lk(m_);
        while (free_ == 0) {
            cv_.wait(lk);
            if (free_ == 0) wasted_wakeups.fetch_add(1, std::memory_order_relaxed);
        }
        --free_;
    }

    void release() {
        {
            std::unique_lock<std::mutex> lk(m_);
            ++free_;
        }
        // Exactly one slot became free, so exactly one waiter can use it.
        // notify_one wakes one sleeper; since every sleeper's predicate is
        // identical, whichever one the OS picks will find it true.
        cv_.notify_one();
    }

private:
    std::mutex m_;
    std::condition_variable cv_;
    int free_;
};
