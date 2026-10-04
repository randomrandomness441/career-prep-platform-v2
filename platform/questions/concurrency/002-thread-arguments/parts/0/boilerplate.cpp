#include <functional>
#include <thread>

struct Counter { int value = 0; };

void add_n(Counter& c, int n) {
    for (int i = 0; i < n; ++i) ++c.value;
}

void launch_lambda(Counter& c, int n) {
    // TODO: this compiles, runs, and leaves the caller's counter at 0.
    //       Why? Fix it without changing anything else.
    std::thread t([c, n]() mutable { add_n(c, n); });
    t.join();
}

void launch_function(Counter& c, int n) {
    // TODO: start a thread that runs add_n on the CALLER's counter, then join.
    //       Writing std::thread t(add_n, c, n); will not compile — work out why
    //       before you look at the hints.
}
