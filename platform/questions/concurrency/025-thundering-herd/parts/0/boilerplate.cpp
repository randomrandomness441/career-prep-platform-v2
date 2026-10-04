#include <atomic>
#include <condition_variable>
#include <mutex>

// A bounded resource pool: a fixed number of slots, any number of threads
// may acquire() and release() them concurrently.
//
// wasted_wakeups is test-only instrumentation, not part of the "real" API:
// it counts how many times a waiting thread woke up, rechecked, and found
// nothing for it — exactly the thing this question is about measuring.
class SlotPool {
public:
    explicit SlotPool(int slots) : free_(slots) {}

    std::atomic<long> wasted_wakeups{0};
    std::atomic<long> started{0};   // test uses this to know all threads are running

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
        // TODO: exactly one slot just became free. How many waiters can
        //       possibly use it? Waking everyone means everyone but the
        //       winner locks the mutex, finds free_ == 0 again, and goes
        //       straight back to sleep — for nothing.
        cv_.notify_all();
    }

private:
    std::mutex m_;
    std::condition_variable cv_;
    int free_;
};
