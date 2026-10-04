#include <condition_variable>
#include <functional>
#include <mutex>

class H2O {
    std::mutex m_;
    std::condition_variable cv_;

    // How much of the molecule currently under construction is already out.
    int h_ = 0;
    int o_ = 0;

    // Called with the mutex held. A finished molecule resets the slots so the
    // threads waiting behind it can go.
    void start_next_if_complete() {
        if (h_ == 2 && o_ == 1) {
            h_ = 0;
            o_ = 0;
        }
    }

public:
    H2O() = default;

    void hydrogen(std::function<void()> releaseHydrogen) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return h_ < 2; });

        // The release happens INSIDE the critical section, between the check and
        // the count. That is the whole exercise: the counters describe the output
        // only if no other thread can emit an atom in the gap between them.
        releaseHydrogen();
        ++h_;

        start_next_if_complete();
        cv_.notify_all();
    }

    void oxygen(std::function<void()> releaseOxygen) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return o_ < 1; });

        releaseOxygen();
        ++o_;

        start_next_if_complete();
        cv_.notify_all();
    }
};
