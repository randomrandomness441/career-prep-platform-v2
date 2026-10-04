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

// A fixed-capacity queue that never grows without bound. What happens when
// it's full is up to `policy`: Block makes the producer wait, DropNewest
// refuses the new item, DropOldest evicts the head to make room. Every drop
// must be counted in dropped().
template <typename T>
class backpressure_queue {
    mutable std::mutex m_;
    std::condition_variable not_empty_;

    std::deque<T> q_;
    const std::size_t capacity_;
    const overflow_policy policy_;

    std::atomic<bool> closed_{false};
    std::atomic<std::size_t> dropped_{0};

public:
    backpressure_queue(std::size_t capacity, overflow_policy policy)
        : capacity_(capacity == 0 ? 1 : capacity), policy_(policy) {}

    backpressure_queue(const backpressure_queue&) = delete;
    backpressure_queue& operator=(const backpressure_queue&) = delete;

    // Returns true if the item is now in the queue. Returns false if it was
    // refused (DropNewest at capacity, or the queue is closed).
    bool push(T v) {
        // TODO: implement
        (void)v;
        return false;
    }

    // Blocks until an item is available. nullopt means closed and drained.
    std::optional<T> pop() {
        // TODO: implement
        return std::nullopt;
    }

    std::optional<T> try_pop() {
        // TODO: implement
        return std::nullopt;
    }

    void close() {
        // TODO: implement
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(m_);
        return q_.size();
    }

    std::size_t capacity() const { return capacity_; }
    std::size_t dropped() const { return dropped_.load(std::memory_order_relaxed); }
    overflow_policy policy() const { return policy_; }
};
