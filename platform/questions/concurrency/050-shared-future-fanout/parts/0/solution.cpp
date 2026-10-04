#include <functional>
#include <future>
#include <utility>

// SharedComputation<T> runs `work` once, asynchronously, and lets any number
// of threads read the result any number of times.
//
// The trick is std::shared_future<T> instead of std::future<T>. A plain
// future's get() moves the result out and invalidates the future -- it is a
// single-use ticket. shared_future::get() is const: it hands back a
// reference to (or copy of) a value that is already sitting in the shared
// state, and reading the same already-published value from many threads at
// once needs no synchronisation at all. That is what makes concurrent get()
// calls on one shared_future object safe.
template <typename T>
class SharedComputation {
public:
    explicit SharedComputation(std::function<T()> work)
        // std::async always hands back a plain std::future; .share() converts
        // it once, here, at construction time. Everyone downstream only ever
        // sees the shared_future.
        : fut_(std::async(std::launch::async, std::move(work)).share()) {}

    // const: get() never mutates *this. Any number of threads may call this
    // on the same object at the same time.
    T get() const { return fut_.get(); }

private:
    std::shared_future<T> fut_;
};
