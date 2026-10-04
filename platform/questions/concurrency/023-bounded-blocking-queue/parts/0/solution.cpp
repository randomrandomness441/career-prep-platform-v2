#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

class bounded_queue {
    // `mutable` so the const observers below can still lock it.
    mutable std::mutex m_;

    // Two waiting rooms, one per condition. A producer only ever sleeps on
    // not_full_; a consumer only ever sleeps on not_empty_. That is what makes
    // notify_one safe: the one thread woken is guaranteed to be a thread that
    // cares about the change that just happened.
    std::condition_variable not_full_;
    std::condition_variable not_empty_;

    std::queue<int> q_;
    const std::size_t capacity_;

    // Atomic so a reader outside the lock could poll it, but note carefully:
    // the atomicity is NOT what makes close() safe. See close().
    std::atomic<bool> closed_{false};

public:
    explicit bounded_queue(std::size_t capacity)
        // A capacity of zero would mean "never has room", so every enqueue
        // would block forever. Refuse to build that.
        : capacity_(capacity == 0 ? 1 : capacity) {}

    bounded_queue(const bounded_queue&) = delete;
    bounded_queue& operator=(const bounded_queue&) = delete;

    // Blocks while the queue is full. Returns false if the queue was closed
    // instead of making room.
    bool enqueue(int v) {
        std::unique_lock<std::mutex> lk(m_);
        // q_.size() is std::size_t and capacity_ is std::size_t. Comparing a
        // size_t against an int here is the classic sign-compare warning, and
        // -Wall -Wextra will reject it.
        not_full_.wait(lk, [this] { return q_.size() < capacity_ || closed_.load(); });
        if (closed_.load()) return false;
        q_.push(std::move(v));

        // Unlock BEFORE notifying. If we notified while still holding m_, the
        // thread we wake would return from wait() straight into a blocked
        // lock() on the mutex we have not released yet.
        lk.unlock();
        not_empty_.notify_one();
        return true;
    }

    // Blocks while the queue is empty. Returns nullopt only when the queue is
    // closed AND drained -- a closed queue still hands out what is left in it.
    std::optional<int> dequeue() {
        std::unique_lock<std::mutex> lk(m_);
        not_empty_.wait(lk, [this] { return !q_.empty() || closed_.load(); });
        if (q_.empty()) return std::nullopt;   // closed and empty: end of stream
        int v = std::move(q_.front());
        q_.pop();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }

    // Non-blocking variants, for callers that must not park.
    bool try_enqueue(int v) {
        std::unique_lock<std::mutex> lk(m_);
        if (closed_.load() || q_.size() >= capacity_) return false;
        q_.push(std::move(v));
        lk.unlock();
        not_empty_.notify_one();
        return true;
    }

    std::optional<int> try_dequeue() {
        std::unique_lock<std::mutex> lk(m_);
        if (q_.empty()) return std::nullopt;
        int v = std::move(q_.front());
        q_.pop();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }

    // Wake everyone, permanently. Both predicates mention closed_, so every
    // parked thread re-evaluates and finds a reason to leave.
    void close() {
        {
            // Taking the mutex here is required even though closed_ is atomic.
            // Without it: a thread evaluates the predicate (false), and before
            // it can park, we set the flag and notify. The notification lands
            // on an empty waiting room, the thread parks a moment later, and it
            // sleeps forever on a signal that has already been spent. Holding
            // the mutex across the store makes "check predicate" and "set flag"
            // mutually exclusive, which closes that window.
            std::lock_guard<std::mutex> lk(m_);
            closed_.store(true);
        }
        // notify_ALL here, not notify_one: shutdown must reach every waiter,
        // and there is no bound on how many there are.
        not_full_.notify_all();
        not_empty_.notify_all();
    }

    bool is_closed() const { return closed_.load(); }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(m_);
        return q_.size();
    }

    std::size_t capacity() const { return capacity_; }
};
