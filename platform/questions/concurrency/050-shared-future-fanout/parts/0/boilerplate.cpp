#include <functional>
#include <future>
#include <utility>

// Runs `work` once, asynchronously, and lets any number of threads call
// get() to read the result, any number of times each, from any of them.
template <typename T>
class SharedComputation {
public:
    explicit SharedComputation(std::function<T()> work)
        : fut_(std::async(std::launch::async, std::move(work))) {}

    // TODO: std::future<T>::get() may only be called once, ever. What type
    //       in <future> is meant to be read more than once, by more than
    //       one thread?
    T get() { return fut_.get(); }

private:
    std::future<T> fut_;
};
