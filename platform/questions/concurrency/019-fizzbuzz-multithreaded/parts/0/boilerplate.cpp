#include <condition_variable>
#include <functional>
#include <mutex>

// Four threads (fizz, buzz, fizzbuzz, number) take turns printing 1..n in
// order, each one only for the values that belong to it.
class FizzBuzz {
    int n;
    int current = 1;                 // guarded by m
    std::mutex m;
    std::condition_variable cv;

    void run(bool (*mine)(int), const std::function<void(int)>& emit) {
        while (true) {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [&] { return current > n || mine(current); });
            if (current > n) return;
            emit(current);
            ++current;
            lk.unlock();

            // TODO: four threads are asleep on this one condition variable,
            //       each waiting for a different predicate. What has to
            //       happen here so the right one of them actually wakes up?
            cv.notify_one();
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
