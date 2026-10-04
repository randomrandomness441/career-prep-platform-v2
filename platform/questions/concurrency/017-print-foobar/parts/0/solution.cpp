#include <condition_variable>
#include <functional>
#include <mutex>

class FooBar {
    int n;
    std::mutex m;
    std::condition_variable cv;
    bool foo_turn = true;      // guarded by m; foo prints first

public:
    FooBar(int n_) : n(n_) {}

    void foo(std::function<void()> printFoo) {
        for (int i = 0; i < n; ++i) {
            std::unique_lock<std::mutex> lk(m);
            // Predicate form: re-checks foo_turn on every wakeup, so a wakeup
            // that was spurious (or meant for someone else) sends us back to
            // sleep instead of printing out of turn.
            cv.wait(lk, [this] { return foo_turn; });
            printFoo();
            foo_turn = false;
            lk.unlock();           // unlock before notifying: the woken thread
            cv.notify_one();       // would otherwise wake straight onto a held mutex
        }
    }

    void bar(std::function<void()> printBar) {
        for (int i = 0; i < n; ++i) {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [this] { return !foo_turn; });
            printBar();
            foo_turn = true;
            lk.unlock();
            cv.notify_one();
        }
    }
};
