#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// An async executor for a common microservice shape: process events off
// the hot path, on a fixed worker pool, BUT events that share a key (the
// same user, the same order, the same aggregate) must still run in
// submission order relative to each other -- while events for different
// keys run freely in parallel across the pool.
//
// The trick: at most one "ready to run" unit of work per key is ever sitting
// in the shared ready queue at a time. A key's second submitted task
// doesn't go into the ready queue directly -- it waits in that key's own
// pending list until the key's currently-running task finishes and
// re-enqueues the next one itself. That single invariant ("never more than
// one ready dispatch per key") is what makes same-key tasks serialize
// without needing a lock held across the actual task execution.
class KeyedAsyncExecutor {
public:
    explicit KeyedAsyncExecutor(int num_workers) {
        for (int i = 0; i < num_workers; ++i) workers_.emplace_back([this] { run(); });
    }

    ~KeyedAsyncExecutor() {
        {
            std::lock_guard<std::mutex> lk(m_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    void submit(std::string key, std::function<void()> task) {
        std::lock_guard<std::mutex> lk(m_);
        pending_[key].push_back(std::move(task));
        if (active_.insert(key).second) {
            // This key had nothing in flight -- its first task can go
            // straight into the ready queue.
            ready_.push(make_dispatch(key));
            cv_.notify_one();
        }
        // Otherwise a dispatch for this key is already running or queued;
        // it will pick up this new task itself once it gets there.
    }

private:
    // Runs exactly one pending task for `key`, then either re-enqueues
    // itself (via a fresh dispatch) if more work for this key arrived, or
    // marks the key idle.
    std::function<void()> make_dispatch(std::string key) {
        return [this, key] {
            std::function<void()> task;
            {
                std::lock_guard<std::mutex> lk(m_);
                task = std::move(pending_[key].front());
                pending_[key].pop_front();
            }
            task();
            {
                std::lock_guard<std::mutex> lk(m_);
                if (pending_[key].empty()) {
                    pending_.erase(key);
                    active_.erase(key);
                } else {
                    ready_.push(make_dispatch(key));
                    cv_.notify_one();
                }
            }
        };
    }

    void run() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [&] { return !ready_.empty() || stop_; });
                if (ready_.empty()) return;  // stop_ true and nothing left
                job = std::move(ready_.front());
                ready_.pop();
            }
            job();
        }
    }

    std::mutex m_;
    std::condition_variable cv_;
    std::unordered_map<std::string, std::deque<std::function<void()>>> pending_;
    std::unordered_set<std::string> active_;
    std::queue<std::function<void()>> ready_;
    bool stop_ = false;
    std::vector<std::thread> workers_;
};
