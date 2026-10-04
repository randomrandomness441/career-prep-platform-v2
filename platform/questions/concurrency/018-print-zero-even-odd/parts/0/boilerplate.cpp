#include <functional>
#include <semaphore>

// Three threads share one sequence: 0, 1, 0, 2, 0, 3, 0, 4, ... up to n.
// zero() always goes between two numbers; even() and odd() print every other
// number, in order. Only one of the three may be running at any moment.
class ZeroEvenOdd {
    int n;

    // TODO: what state do you need so zero(), even(), and odd() each run at
    //       exactly the right moment, and never more than one at a time?

public:
    ZeroEvenOdd(int n_) : n(n_) {}

    void zero(std::function<void(int)> printNumber) {
        for (int i = 1; i <= n; ++i) {
            // TODO: wait for zero's turn, print 0, then hand off to whichever
            //       of even/odd should run next.
            printNumber(0);
        }
    }

    void even(std::function<void(int)> printNumber) {
        for (int i = 2; i <= n; i += 2) {
            // TODO: wait for even's turn, print i, then hand off to zero.
            printNumber(i);
        }
    }

    void odd(std::function<void(int)> printNumber) {
        for (int i = 1; i <= n; i += 2) {
            // TODO: wait for odd's turn, print i, then hand off to zero.
            printNumber(i);
        }
    }
};
