#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// A work-stealing thread pool: each worker owns its own deque of tasks,
// pushing and popping from its own end, and steals from the opposite end
// of another worker's deque when it runs out of its own work.
class WorkStealingPool {
public:
    explicit WorkStealingPool(int num_threads) : queues_(static_cast<std::size_t>(num_threads)) {
        for (auto& q : queues_) q = std::make_unique<Queue>();
        for (int i = 0; i < num_threads; ++i)
            workers_.emplace_back([this, i] { run(i); });
    }

    // TODO: what else does this destructor need to do before workers_
    //       itself gets destroyed?
    ~WorkStealingPool() {
        stop_.store(true, std::memory_order_release);
    }

    void submit(std::function<void()> task) {
        std::size_t i = static_cast<std::size_t>(
            next_.fetch_add(1, std::memory_order_relaxed)) % queues_.size();
        queues_[i]->push(std::move(task));
    }

private:
    struct Queue {
        std::mutex m;
        std::deque<std::function<void()>> tasks;

        void push(std::function<void()> f) {
            std::lock_guard<std::mutex> lk(m);
            tasks.push_back(std::move(f));
        }
        bool pop_own(std::function<void()>& out) {
            std::lock_guard<std::mutex> lk(m);
            if (tasks.empty()) return false;
            out = std::move(tasks.back());
            tasks.pop_back();
            return true;
        }
        bool steal(std::function<void()>& out) {
            std::lock_guard<std::mutex> lk(m);
            if (tasks.empty()) return false;
            out = std::move(tasks.front());
            tasks.pop_front();
            return true;
        }
    };

    void run(int self) {
        while (true) {
            std::function<void()> task;
            if (queues_[static_cast<std::size_t>(self)]->pop_own(task)) {
                task();
                continue;
            }
            bool stole = false;
            for (std::size_t off = 1; off < queues_.size(); ++off) {
                std::size_t victim = (static_cast<std::size_t>(self) + off) % queues_.size();
                if (queues_[victim]->steal(task)) {
                    stole = true;
                    task();
                    break;
                }
            }
            if (stole) continue;

            if (stop_.load(std::memory_order_acquire)) return;
            std::this_thread::yield();
        }
    }

    std::vector<std::unique_ptr<Queue>> queues_;
    std::vector<std::thread> workers_;
    std::atomic<int> next_{0};
    std::atomic<bool> stop_{false};
};
