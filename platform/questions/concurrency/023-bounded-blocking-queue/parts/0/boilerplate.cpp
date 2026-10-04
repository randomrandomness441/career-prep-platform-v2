#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

// A fixed-capacity queue, safe for any number of producers and consumers.
// enqueue() blocks while full, dequeue() blocks while empty. close() wakes
// everyone up for good: enqueue() then fails, and dequeue() keeps returning
// whatever is left until the queue drains, then returns nullopt.
class bounded_queue {
    mutable std::mutex m_;
    std::condition_variable cv_;
    std::queue<int> q_;
    const std::size_t capacity_;
    std::atomic<bool> closed_{false};

public:
    explicit bounded_queue(std::size_t capacity)
        : capacity_(capacity == 0 ? 1 : capacity) {}

    bounded_queue(const bounded_queue&) = delete;
    bounded_queue& operator=(const bounded_queue&) = delete;

    bool enqueue(int v) {
        // TODO: implement
        (void)v;
        return false;
    }

    std::optional<int> dequeue() {
        // TODO: implement
        return std::nullopt;
    }

    bool try_enqueue(int v) {
        // TODO: implement
        (void)v;
        return false;
    }

    std::optional<int> try_dequeue() {
        // TODO: implement
        return std::nullopt;
    }

    void close() {
        // TODO: implement
    }

    bool is_closed() const { return closed_.load(); }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(m_);
        return q_.size();
    }

    std::size_t capacity() const { return capacity_; }
};
