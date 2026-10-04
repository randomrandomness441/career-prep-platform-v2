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

// A fixed-size pool of persistent worker threads pulling from one shared
// task queue. submit() hands back a std::future the caller waits on for the
// result; the pool owns nothing the caller needs to manage by hand.
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

    // Graceful shutdown: every task already in the queue when the pool is
    // destroyed still runs to completion. No submitted task is ever silently
    // dropped, no matter how many are queued at the moment of destruction.
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lk(m_);
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    // submit()'s signature is the price of "works for any callable, any
    // arguments" -- not a concurrency concept; see 007's reading for the
    // F&&/std::forward/invoke_result_t glossary if this syntax is unfamiliar.
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;

        // packaged_task is move-only; std::function requires its target to
        // be copyable, so it can't hold a packaged_task directly. A
        // shared_ptr makes the wrapping lambda copyable while the task
        // itself is only ever moved-into once, at construction.
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

                // Check the QUEUE first, not the stop flag. stopping_ can be
                // true while work is still waiting to run; the predicate
                // above only guarantees "stopping_ or something to do", not
                // "nothing to do". Only exit once the queue is actually
                // drained -- that's what makes shutdown graceful instead of
                // a way to lose the last batch of submitted work.
                if (queue_.empty()) return;   // queue empty here implies stopping_
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
