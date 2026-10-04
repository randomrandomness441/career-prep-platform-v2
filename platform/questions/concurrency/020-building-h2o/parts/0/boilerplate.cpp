#include <condition_variable>
#include <functional>
#include <mutex>

// Many threads call hydrogen() and oxygen(). Atoms must be released two at
// a time from hydrogen and one at a time from oxygen, grouped into
// molecules: exactly two hydrogens and one oxygen released, in any order
// relative to each other, before the next molecule's atoms may start.
class H2O {
    std::mutex m_;
    std::condition_variable cv_;

    // TODO 1: you need to know how much of the current molecule is already out.
    //         Two counters, hydrogen and oxygen, both starting at zero.

public:
    H2O() = default;

    void hydrogen(std::function<void()> releaseHydrogen) {
        std::unique_lock<std::mutex> lk(m_);
        // TODO 2: a hydrogen may only go out if this molecule still has room for
        //         one. Wait until that is true instead of barging through.
        releaseHydrogen();
        // TODO 3: record it, and when the molecule is complete, start a new one
        //         and wake whoever is waiting.
        cv_.notify_all();
    }

    void oxygen(std::function<void()> releaseOxygen) {
        std::unique_lock<std::mutex> lk(m_);
        // TODO 4: same idea, but a molecule has room for only one oxygen.
        releaseOxygen();
        cv_.notify_all();
    }
};
