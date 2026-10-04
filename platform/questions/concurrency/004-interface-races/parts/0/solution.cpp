#include <mutex>
#include <optional>
#include <utility>
#include <vector>

class threadsafe_stack {
    mutable std::mutex m_;
    std::vector<int> data_;

public:
    threadsafe_stack() = default;
    threadsafe_stack(const threadsafe_stack&) = delete;
    threadsafe_stack& operator=(const threadsafe_stack&) = delete;

    void push(int value) {
        std::lock_guard<std::mutex> g(m_);
        data_.push_back(value);
    }

    // "Is there something there?" and "give it to me and remove it" are one
    // operation, under one lock. No other thread can slip between the check
    // and the removal, because there is no moment between them where the
    // lock is not held.
    //
    // The empty case is reported in the return value instead of by a
    // separate query, so the caller never has to ask twice.
    std::optional<int> pop() {
        std::lock_guard<std::mutex> g(m_);
        if (data_.empty()) return std::nullopt;
        std::optional<int> result(data_.back());
        data_.pop_back();
        return result;
    }

    // Advisory only. By the time the caller reads this answer it may already
    // be stale, so it is useful for logging and metrics and for nothing else.
    // There is deliberately no top(): a value read without being removed is a
    // value another thread may already have taken.
    bool empty() const {
        std::lock_guard<std::mutex> g(m_);
        return data_.empty();
    }
};
