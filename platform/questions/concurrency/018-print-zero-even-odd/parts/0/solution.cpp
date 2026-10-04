#include <functional>
#include <semaphore>

class ZeroEvenOdd {
    int n;

    // One gate per thread. zero() starts open, everyone else starts shut, so
    // the sequence begins with a 0 no matter which thread the OS runs first.
    std::binary_semaphore sem_zero{1};
    std::binary_semaphore sem_even{0};
    std::binary_semaphore sem_odd{0};

public:
    ZeroEvenOdd(int n_) : n(n_) {}

    void zero(std::function<void(int)> printNumber) {
        for (int i = 1; i <= n; ++i) {
            sem_zero.acquire();
            printNumber(0);
            // Route the wakeup: i is the number that comes next, so its parity
            // decides which of the two number threads is allowed to run.
            if (i % 2 == 1) sem_odd.release();
            else            sem_even.release();
        }
    }

    void even(std::function<void(int)> printNumber) {
        for (int i = 2; i <= n; i += 2) {
            sem_even.acquire();
            printNumber(i);
            // Released by a thread that never acquired it. A mutex cannot do this.
            sem_zero.release();
        }
    }

    void odd(std::function<void(int)> printNumber) {
        for (int i = 1; i <= n; i += 2) {
            sem_odd.acquire();
            printNumber(i);
            sem_zero.release();
        }
    }
};
