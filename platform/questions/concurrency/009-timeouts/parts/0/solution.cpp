#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <utility>

// A concrete int slot -- the mechanics here (deadline vs duration, steady vs
// system clock) don't depend on what type is inside, so there's no need to
// templatize it.
class result_slot {
    std::mutex m_;
    std::condition_variable cv_;
    std::optional<int> value_;

public:
    void set(int v) {
        {
            std::lock_guard<std::mutex> lk(m_);
            value_ = std::move(v);
        }
        cv_.notify_all();   // notify outside the lock: the woken thread would
                            // only have to block on the mutex again.
    }

    void nudge() { cv_.notify_all(); }

    std::optional<int> wait_for_result(std::chrono::milliseconds timeout) {
        // The caller's patience is a point in time, fixed here and never moved.
        // steady_clock, because system_clock can be stepped backwards by NTP or
        // by a user, and a deadline on a clock that runs backwards drifts away
        // from you while you wait on it.
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        std::unique_lock<std::mutex> lk(m_);
        // wait_until re-enters with the time that is LEFT, so a wakeup at 10 ms
        // sleeps another 40, not another 50. The predicate overload returns the
        // predicate's final value, so false means "deadline passed, still empty",
        // and it evaluates the predicate before sleeping, which is what makes a
        // 0 ms timeout still see a value that is already there.
        if (!cv_.wait_until(lk, deadline, [this] { return value_.has_value(); }))
            return std::nullopt;
        return value_;
    }
};
