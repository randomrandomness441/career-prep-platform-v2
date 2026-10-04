#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stop_token>
#include <thread>

class queue_worker {
    // Declaration order matters: members are destroyed in reverse order, so
    // the jthread declared LAST is destroyed FIRST — its stop+join runs while
    // the mutex, condition variable and queue are still alive.
    std::mutex m_;
    std::condition_variable_any cv_;  // NOT std::condition_variable: the wait
                                      // overload taking a stop_token exists
                                      // only on condition_variable_any
    std::deque<long> jobs_;           // guarded by m_
    std::atomic<long> sum_{0};
    std::atomic<int> processed_{0};
    std::jthread th_;                 // destroyed first: request_stop + join

    void run(std::stop_token st) {
        std::unique_lock<std::mutex> lk(m_);
        // wait(lock, stop_token, pred) returns pred() — true when a job is
        // available, false when stop was requested (with the predicate false).
        // Internally the wait registers a stop callback that notifies the cv,
        // so a parked thread leaves the wait the moment request_stop() fires.
        while (cv_.wait(lk, st, [this] { return !jobs_.empty(); })) {
            while (!jobs_.empty()) {          // drain what is queued
                long v = jobs_.front();
                jobs_.pop_front();
                sum_.fetch_add(v, std::memory_order_relaxed);
                processed_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

public:
    queue_worker() : th_([this](std::stop_token st) { run(st); }) {}

    // jthread's destructor does request_stop() then join() — nothing to write.
    // Safe here because th_ is declared last (see above).

    void submit(long job) {
        {
            std::lock_guard<std::mutex> lk(m_);
            jobs_.push_back(job);
        }
        cv_.notify_all();
    }

    void request_stop() { th_.request_stop(); }

    void join() {
        if (th_.joinable()) th_.join();
    }

    long sum() const { return sum_.load(std::memory_order_relaxed); }
    int processed() const { return processed_.load(std::memory_order_relaxed); }
};
