#include <functional>
#include <semaphore>

class Foo {
    std::binary_semaphore gate2{0};
    std::binary_semaphore gate3{0};

    // Destructors run during stack unwinding, so the gate opens whether we
    // leave normally or via an exception. Preferred over try/catch because it
    // cannot be forgotten and survives someone adding an early return later.
    struct Opener {
        std::binary_semaphore& s;
        ~Opener() { s.release(); }
    };

public:
    Foo() {}

    void first(std::function<void()> printFirst) {
        Opener o{gate2};
        printFirst();          // if this throws, ~Opener still releases gate2
    }

    void second(std::function<void()> printSecond) {
        gate2.acquire();
        Opener o{gate3};
        printSecond();
    }

    void third(std::function<void()> printThird) {
        gate3.acquire();
        printThird();
    }
};
