#include <atomic>
#include <memory>
#include <thread>

struct Job { int id; };

std::thread run_job(std::unique_ptr<Job> job, std::atomic<int>* out) {
    // TODO: this does not compile. std::thread copies its arguments, and a
    //       unique_ptr cannot be copied. Transfer ownership instead.
    return std::thread([](std::unique_ptr<Job> j, std::atomic<int>* o) {
        o->store(j->id);
    }, job, out);
}
