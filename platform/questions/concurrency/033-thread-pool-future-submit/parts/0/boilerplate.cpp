#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// A pool of persistent worker threads, each pulling from one shared task
// queue. submit() hands back a std::future for whatever the callable
// returns. Shutdown must let every already-queued task actually run before
// the pool's threads exit.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t n_threads) {
        workers_.reserve(n_threads);
        for (std::size_t i = 0; i < n_threads; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lk(m_);
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    // submit()'s signature is the price of "works for any callable, any
    // arguments" -- not a concurrency concept. F&&/Args&&... is a forwarding
    // reference pack ("bind to whatever was passed, remember move vs copy");
    // std::forward passes each one along the way it arrived; invoke_result_t
    // asks the compiler "what type does calling f(args...) produce?" so the
    // return type can be written without you naming it. See 007's reading
    // for the same glossary, written out in full.
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<R> result = task->get_future();
        {
            std::lock_guard<std::mutex> lk(m_);
            queue_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [this] { return stopping_ || !queue_.empty(); });

                // TODO: is stopping_ the right thing to check here, on
                //       its own, given there might still be queued work?
                if (stopping_) return;
                task = std::move(queue_.front());
                queue_.pop();
            }
            task();
        }
    }

    std::mutex m_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> queue_;
    bool stopping_ = false;
    std::vector<std::thread> workers_;
};
