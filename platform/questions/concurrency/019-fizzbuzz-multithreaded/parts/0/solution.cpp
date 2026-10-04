#include <condition_variable>
#include <functional>
#include <mutex>

class FizzBuzz {
    int n;
    int current = 1;                 // guarded by m; the value waiting to be printed
    std::mutex m;
    std::condition_variable cv;

    // All four methods are the same loop with a different predicate and a
    // different thing to print.
    void run(bool (*mine)(int), const std::function<void(int)>& emit) {
        while (true) {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [&] { return current > n || mine(current); });
            if (current > n) return;         // sequence finished; go home
            emit(current);
            ++current;
            lk.unlock();

            // notify_all, not notify_one. Four threads sleep on this one
            // condition variable, each with a DIFFERENT predicate. notify_one
            // is free to wake a thread whose predicate is still false; that
            // thread rechecks, goes back to sleep, and the notification is
            // spent — nobody else was told. Every thread then sleeps forever.
            // Waking all four costs three pointless wakeups per number and is
            // the price of using a single condition variable for four
            // different conditions. See reading sections 3 and 6.
            cv.notify_all();
        }
    }

public:
    FizzBuzz(int n_) : n(n_) {}

    void fizz(std::function<void()> printFizz) {
        run([](int v) { return v % 3 == 0 && v % 5 != 0; },
            [&](int) { printFizz(); });
    }

    void buzz(std::function<void()> printBuzz) {
        run([](int v) { return v % 5 == 0 && v % 3 != 0; },
            [&](int) { printBuzz(); });
    }

    void fizzbuzz(std::function<void()> printFizzBuzz) {
        run([](int v) { return v % 15 == 0; },
            [&](int) { printFizzBuzz(); });
    }

    void number(std::function<void(int)> printNumber) {
        run([](int v) { return v % 3 != 0 && v % 5 != 0; },
            [&](int v) { printNumber(v); });
    }
};
