#include <functional>
#include <mutex>
#include <utility>
#include <vector>

class EventGate {
    std::mutex m_;
    bool event_active_ = false;
    std::vector<std::function<void()>> pending_;

public:
    EventGate() = default;

    void begin_event() {
        std::lock_guard<std::mutex> lk(m_);
        event_active_ = true;
    }

    // "Check the flag" and "act on it" happen as one indivisible step, under m_ --
    // that's what makes it impossible for a callback to land in a waiting list that
    // end_event() has already drained and forgotten about.
    void reg_cb(std::function<void()> cb) {
        std::unique_lock<std::mutex> lk(m_);
        if (event_active_) {
            pending_.push_back(std::move(cb));
            return;   // queued; end_event() will run it later
        }
        lk.unlock();
        cb();         // no event in progress: run immediately, lock already released
    }

    void end_event() {
        std::vector<std::function<void()>> to_run;
        {
            std::lock_guard<std::mutex> lk(m_);
            event_active_ = false;
            to_run.swap(pending_);   // empty pending_ in one O(1) move, under the lock
        }
        // Run every queued callback OUTSIDE the lock: holding m_ here would block every
        // other reg_cb() call for the whole batch, and would deadlock outright if any
        // callback calls reg_cb() itself (reentrant registration -- the interface
        // doesn't forbid it, and by the time this runs event_active_ is already false,
        // so a reentrant reg_cb() call correctly runs immediately instead of re-queuing).
        for (auto& cb : to_run) cb();
    }
};
