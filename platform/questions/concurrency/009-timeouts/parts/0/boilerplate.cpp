#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <utility>

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
        cv_.notify_all();
    }

    void nudge() { cv_.notify_all(); }

    // TODO: block until a value is set or `timeout` elapses, whichever comes
    //       first, and return the value (or nullopt on timeout). A caller
    //       may call nudge() one or more times before the value is ever set.
    //       Think about what "timeout" means to the caller across repeated
    //       wakeups, not just on the very first one.
    std::optional<int> wait_for_result(std::chrono::milliseconds timeout) {
        return std::nullopt;
    }
};
