#include <atomic>
#include <utility>

// A lock-free stack: push and pop from any number of threads at once, no
// mutex anywhere. The hard part isn't push and pop themselves, it's knowing
// when a popped node is actually safe to delete -- some other thread's
// pop() might still be looking at it.
template <typename T>
class LockFreeStack {
public:
    LockFreeStack() = default;

    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;

    ~LockFreeStack() {
        // TODO: implement
    }

    void push(T value) {
        // TODO: implement
        (void)value;
    }

    // Returns false if the stack was empty at the moment we looked.
    bool pop(T& out) {
        // TODO: implement
        (void)out;
        return false;
    }

    bool empty() const {
        // TODO: implement
        return true;
    }

private:
    struct Node {
        explicit Node(T v) : data(std::move(v)) {}
        T data;
        Node* next = nullptr;
    };

    std::atomic<Node*> head_{nullptr};
};
