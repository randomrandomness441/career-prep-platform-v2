#include <functional>
#include <mutex>
#include <utility>
#include <vector>

// While an event is "active" (between begin_event() and end_event()), any
// callback registered via reg_cb() must be queued and run when end_event()
// fires. Outside an active event, reg_cb() runs its callback immediately.
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

    void reg_cb(std::function<void()> cb) {
        // TODO: implement
        (void)cb;
    }

    void end_event() {
        // TODO: implement
    }
};
