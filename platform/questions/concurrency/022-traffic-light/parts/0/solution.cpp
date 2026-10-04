#include <condition_variable>
#include <functional>
#include <mutex>
#include <utility>

// A four-way intersection with one bridge. Two axes:
//   road 0 = north/south, road 1 = east/west.
// Cars on the green axis are on the bridge at the same time. The light
// switches when the bridge empties and the other axis is queued.
class traffic_light {
    std::mutex m_;
    std::condition_variable cv_;
    int green_        = 0;     // axis that currently has the light
    bool closed_      = false; // green axis closed to newcomers (other axis queued)
    int passing_      = 0;     // cars on the bridge right now
    int waiting_[2]   = {0, 0}; // cars arrived but not yet admitted, per axis

    // m_ held. The light may only change while the bridge is empty.
    bool try_switch() {
        if (passing_ != 0) return false;
        if (waiting_[1 - green_] == 0) return false;
        green_ = 1 - green_;
        closed_ = false;
        return true;
    }

    // m_ held. One car leaves the bridge; hand the light over if it emptied.
    void leave_one() {
        if (--passing_ == 0 && try_switch()) cv_.notify_all();
    }

public:
    // Blocks until this car's axis has the light and the axis is open, then
    // runs cross_car() while the car crosses. cross_car() runs WITHOUT m_
    // held, so cars on the same axis cross together.
    void cross(int road, std::function<void()> cross_car) {
        std::unique_lock<std::mutex> lk(m_);
        ++waiting_[road];
        if (green_ != road) closed_ = true;      // my arrival closes the green axis
        if (try_switch()) cv_.notify_all();
        cv_.wait(lk, [&] { return green_ == road && !closed_; });
        --waiting_[road];
        ++passing_;
        lk.unlock();

        // RAII ticket: even if cross_car() throws, the bridge still empties
        // and the light handover still happens.
        struct ticket {
            traffic_light* self;
            ~ticket() {
                std::lock_guard<std::mutex> g(self->m_);
                self->leave_one();
            }
        } on_bridge{this};
        cross_car();
    }
};
