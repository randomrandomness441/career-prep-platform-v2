#include <functional>
#include <semaphore>

class Foo {
    std::binary_semaphore gate2{0};
    std::binary_semaphore gate3{0};

public:
    Foo() {}

    // TODO: if printFirst() throws, gate2 is never released and second()
    //       hangs forever. Make the release happen on both paths, and still
    //       let the exception escape to the caller.
    void first(std::function<void()> printFirst) {
        printFirst();
        gate2.release();
    }

    void second(std::function<void()> printSecond) {
        gate2.acquire();
        printSecond();
        gate3.release();      // TODO: same problem here
    }

    void third(std::function<void()> printThird) {
        gate3.acquire();
        printThird();
    }
};
