#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// A fixed worker pool that runs submitted tasks. Tasks that share the same
// key must run in submission order relative to each other; tasks with
// different keys are free to run in parallel across the pool.
class KeyedAsyncExecutor {
public:
    explicit KeyedAsyncExecutor(int num_workers) {
        // TODO: implement
        (void)num_workers;
    }

    ~KeyedAsyncExecutor() {
        // TODO: implement
    }

    void submit(std::string key, std::function<void()> task) {
        // TODO: implement
        (void)key;
        (void)task;
    }
};
