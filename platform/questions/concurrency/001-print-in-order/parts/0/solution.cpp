#include <functional>
#include <semaphore>

class Foo {
    // Both gates start closed (0 tokens), so second() and third() block
    // immediately no matter which thread the OS runs first.
    std::binary_semaphore gate2{0};
    std::binary_semaphore gate3{0};

public:
    Foo() {}

    void first(std::function<void()> printFirst) {
        printFirst();
        gate2.release();          // opens the gate second() is waiting at
    }

    void second(std::function<void()> printSecond) {
        gate2.acquire();          // sleeps until first() releases
        printSecond();
        gate3.release();
    }

    void third(std::function<void()> printThird) {
        gate3.acquire();
        printThird();
    }
};
