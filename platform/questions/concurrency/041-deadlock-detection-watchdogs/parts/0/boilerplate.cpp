#include <chrono>
#include <functional>
#include <future>
#include <thread>
#include <utility>

// Runs `op` and gives it a deadline. If op() doesn't finish within
// `deadline`, this must return false instead of waiting forever -- even
// though standard C++ gives you no way to forcibly stop another thread.
class Watchdog {
public:
    bool run_with_deadline(std::function<void()> op, std::chrono::milliseconds deadline) {
        // TODO: implement
        (void)deadline;
        op();
        return true;
    }
};
