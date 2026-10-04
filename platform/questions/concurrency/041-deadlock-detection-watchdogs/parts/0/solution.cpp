#include <chrono>
#include <functional>
#include <future>
#include <thread>
#include <utility>

// Watchdog::run_with_deadline runs an operation you expect to finish, and is
// prepared to notice if it doesn't. Standard C++ has no way to forcibly stop
// another thread, so "detect and report" is the honest contract here, not
// "detect and fix" -- there is no safe way to kill a thread that is
// genuinely, permanently stuck (e.g. deadlocked on two mutexes) without
// terminating the whole process.
class Watchdog {
public:
    // Runs `op` on its own thread. If it finishes within `deadline`, joins
    // that thread and returns true. If the deadline elapses first, returns
    // false -- the operation is reported as stalled -- and detaches the
    // thread, because it may genuinely never finish and there is nothing
    // else that can be done with it.
    bool run_with_deadline(std::function<void()> op, std::chrono::milliseconds deadline) {
        auto done = std::make_shared<std::promise<void>>();
        std::future<void> signal = done->get_future();

        // A plain std::thread, not std::async: std::async's future has a
        // special rule that its destructor BLOCKS until the task finishes if
        // nobody has called get() on it. That would defeat the entire point
        // here -- we specifically need to walk away from an operation that
        // may never finish. A raw thread we can detach has no such rule.
        std::thread worker([op = std::move(op), done]() mutable {
            op();
            done->set_value();
        });

        // wait_for returns as soon as the promise is fulfilled OR the
        // deadline passes, whichever comes first -- it does not block for
        // the full deadline if the operation finishes early.
        std::future_status status = signal.wait_for(deadline);
        if (status == std::future_status::ready) {
            worker.join();
            return true;
        }

        // The deadline won. We cannot safely join a thread that might be
        // stuck forever without hanging the watchdog too -- detach it and
        // report the stall. If `op` really is deadlocked, this thread (and
        // whatever mutexes it holds) leaks for the life of the process.
        worker.detach();
        return false;
    }
};
