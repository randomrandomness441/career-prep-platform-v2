#include <functional>

// Two threads, foo() and bar(), must print alternately: foo bar foo bar...
// n times each, no matter how the OS schedules them.
class FooBar {
    int n;

    // TODO 1: what shared state records whose turn it is? One bool is enough.
    // TODO 2: what protects that bool, and what puts a thread to sleep until
    //         the bool says it may go?

public:
    FooBar(int n_) : n(n_) {}

    void foo(std::function<void()> printFoo) {
        for (int i = 0; i < n; ++i) {
            // TODO 3: wait here until it is foo's turn.
            printFoo();
            // TODO 4: hand the turn to bar and wake it.
        }
    }

    void bar(std::function<void()> printBar) {
        for (int i = 0; i < n; ++i) {
            // TODO 5: wait here until it is bar's turn.
            printBar();
            // TODO 6: hand the turn back to foo and wake it.
        }
    }
};
