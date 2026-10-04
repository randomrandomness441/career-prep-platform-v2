#include <condition_variable>
#include <functional>
#include <mutex>

class Ordered {
    int n;
    // TODO: one mutex, one condition_variable, one integer for whose turn it is

public:
    explicit Ordered(int n) : n(n) {}

    // Must call print() only after every id < this one has printed.
    void go(int id, std::function<void()> print) {
        // TODO
        print();
    }
};
