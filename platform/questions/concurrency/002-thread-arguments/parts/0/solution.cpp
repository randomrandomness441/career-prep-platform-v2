#include <functional>
#include <thread>

struct Counter { int value = 0; };

void add_n(Counter& c, int n) {
    for (int i = 0; i < n; ++i) ++c.value;
}

void launch_lambda(Counter& c, int n) {
    // [&c] refers to the caller's object. `mutable` is no longer needed: we are
    // not modifying a member of the closure, we are writing through a reference.
    std::thread t([&c, n] { add_n(c, n); });
    t.join();
}

void launch_function(Counter& c, int n) {
    // std::thread decay-copies its arguments, so a plain `c` produces a Counter
    // rvalue that cannot bind to Counter&. std::ref wraps a pointer in a copyable
    // object; copying the wrapper still leaves it pointing at the caller's object.
    std::thread t(add_n, std::ref(c), n);
    t.join();
}
