#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

// Runs jobs pushed via submit() on a background thread, in the order they
// arrive. request_stop() (or destroying the object) must not leave the
// worker thread parked forever: it has to wake up and exit even if it
// happened to be waiting for more work at the exact moment stop was
// requested.
class queue_worker {
    std::mutex m_;
    std::condition_variable cv_;
    std::deque<long> jobs_;
    std::atomic<long> sum_{0};
    std::atomic<int> processed_{0};

public:
    queue_worker() {
        // TODO: implement
    }

    ~queue_worker() {
        // TODO: implement
    }

    void submit(long job) {
        {
            std::lock_guard<std::mutex> lk(m_);
            jobs_.push_back(job);
        }
        cv_.notify_all();
    }

    void request_stop() {
        // TODO: implement
    }

    void join() {
        // TODO: implement
    }

    long sum() const { return sum_.load(std::memory_order_relaxed); }
    int processed() const { return processed_.load(std::memory_order_relaxed); }
};
