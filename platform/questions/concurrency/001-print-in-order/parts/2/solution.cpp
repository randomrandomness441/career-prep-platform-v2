#include <condition_variable>
#include <functional>
#include <mutex>

class Ordered {
    int n;
    std::mutex m;
    std::condition_variable cv;
    int turn = 1;              // guarded by m

public:
    explicit Ordered(int n) : n(n) {}

    void go(int id, std::function<void()> print) {
        std::unique_lock<std::mutex> lk(m);
        // Sleeps while it is not our turn. The predicate form re-checks on every
        // wake, which is required: threads can wake spuriously, and notify_all
        // wakes everyone regardless of whose turn it now is.
        cv.wait(lk, [&] { return turn == id; });
        print();
        ++turn;
        // notify_all, not notify_one: notify_one may pick a thread whose id is
        // not the new turn. It re-checks, fails, sleeps again — and the signal
        // is consumed without reaching the thread that could actually proceed.
        cv.notify_all();
    }
};
