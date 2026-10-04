#include <functional>
#include <semaphore>

class Foo {
    // TODO: what state do you need so second() and third() can wait?

public:
    Foo() {}

    void first(std::function<void()> printFirst) {
        printFirst();
        // TODO: let second() proceed
    }

    void second(std::function<void()> printSecond) {
        // TODO: wait until first() is done
        printSecond();
        // TODO: let third() proceed
    }

    void third(std::function<void()> printThird) {
        // TODO: wait until second() is done
        printThird();
    }
};
