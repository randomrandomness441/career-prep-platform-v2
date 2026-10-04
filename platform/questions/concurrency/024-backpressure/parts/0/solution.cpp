#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

enum class overflow_policy {
    Block,        // the producer waits. The queue pushes back.
    DropNewest,   // refuse the arriving item, keep the backlog
    DropOldest,   // evict the head, keep the arriving item
};

template <typename T>
class backpressure_queue {
    mutable std::mutex m_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;

    std::deque<T> q_;                  // deque: DropOldest needs pop_front
    const std::size_t capacity_;
    const overflow_policy policy_;

    std::atomic<bool> closed_{false};
    std::atomic<std::size_t> dropped_{0};

public:
    backpressure_queue(std::size_t capacity, overflow_policy policy)
        : capacity_(capacity == 0 ? 1 : capacity), policy_(policy) {}

    backpressure_queue(const backpressure_queue&) = delete;
    backpressure_queue& operator=(const backpressure_queue&) = delete;

    // Returns true if the item is now in the queue.
    // Returns false if it was refused (DropNewest at capacity, or closed).
    bool push(T v) {
        std::unique_lock<std::mutex> lk(m_);

        if (q_.size() >= capacity_ && !closed_.load()) {
            switch (policy_) {
            case overflow_policy::Block:
                // THIS is backpressure. Not an error code, not a log line --
                // the producer simply stops running until there is room. The
                // rate limit propagates upstream for free.
                not_full_.wait(lk, [this] {
                    return q_.size() < capacity_ || closed_.load();
                });
                break;

            case overflow_policy::DropNewest:
                // Shed load deliberately: the backlog survives, the new item
                // does not. Right when old data is still useful (a job queue).
                dropped_.fetch_add(1, std::memory_order_relaxed);
                return false;

            case overflow_policy::DropOldest:
                // Shed load deliberately, the other way round: the freshest
                // item survives. Right when stale data is worthless (a live
                // sensor reading, a UI frame, a position update).
                q_.pop_front();
                dropped_.fetch_add(1, std::memory_order_relaxed);
                break;
            }
        }

        if (closed_.load()) return false;

        q_.push_back(std::move(v));
        lk.unlock();
        not_empty_.notify_one();
        return true;
    }

    // Blocks until an item is available. nullopt means closed and drained.
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lk(m_);
        not_empty_.wait(lk, [this] { return !q_.empty() || closed_.load(); });
        if (q_.empty()) return std::nullopt;
        T v = std::move(q_.front());
        q_.pop_front();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }

    std::optional<T> try_pop() {
        std::unique_lock<std::mutex> lk(m_);
        if (q_.empty()) return std::nullopt;
        T v = std::move(q_.front());
        q_.pop_front();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lk(m_);
            closed_.store(true);
        }
        not_full_.notify_all();
        not_empty_.notify_all();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(m_);
        return q_.size();
    }

    std::size_t capacity() const { return capacity_; }
    std::size_t dropped() const { return dropped_.load(std::memory_order_relaxed); }
    overflow_policy policy() const { return policy_; }
};
