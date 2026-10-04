#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// A work-stealing thread pool: each worker owns its own deque of tasks.
// The owner pushes and pops from the BACK of its own deque (cheap,
// uncontended in the common case, and LIFO -- a worker keeps working on
// whatever it just produced, which tends to be cache-warm). An idle worker
// with nothing of its own steals from the FRONT of some other worker's
// deque instead -- opposite end from the owner, so a steal and the owner's
// own push/pop rarely collide on the same end of the same deque.
//
// Each deque has its own mutex; an operation only ever holds one deque's
// lock at a time, so there's no lock-ordering hazard between workers.
class WorkStealingPool {
public:
    explicit WorkStealingPool(int num_threads) : queues_(static_cast<std::size_t>(num_threads)) {
        for (auto& q : queues_) q = std::make_unique<Queue>();
        for (int i = 0; i < num_threads; ++i)
            workers_.emplace_back([this, i] { run(i); });
    }

    // stop_ tells every worker "no more work is coming," but a worker
    // still keeps checking its own queue and stealing from others before
    // it honours that -- see run() below. Joining here is what makes that
    // meaningful: it gives every worker the chance to actually finish
    // draining before this destructor returns, instead of leaving them
    // running (and still joinable) while workers_ itself is destroyed.
    ~WorkStealingPool() {
        stop_.store(true, std::memory_order_release);
        for (auto& t : workers_) t.join();
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

    // A worker only gives up once BOTH its own queue and every other
    // worker's queue have come back empty in the same pass AND stop_ has
    // been requested -- so as long as some item is still sitting in any
    // queue, some worker's next pass finds and runs it, no matter how
    // close to shutdown that happens. Nothing here can complete until the
    // destructor's join() gives every worker time to actually reach that
    // state.
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
