#include <atomic>
#include <memory>
#include <thread>

struct Job { int id; };

std::thread run_job(std::unique_ptr<Job> job, std::atomic<int>* out) {
    // std::move selects unique_ptr's move constructor, so the pointer is handed
    // to the thread's argument storage and our `job` is left null. The lambda
    // takes it by value, so the thread owns the Job and destroys it on exit.
    return std::thread([](std::unique_ptr<Job> j, std::atomic<int>* o) {
        o->store(j->id);
    }, std::move(job), out);
}
