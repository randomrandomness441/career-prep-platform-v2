#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>

int main() {
    for (int trial = 0; trial < 20; ++trial) {
        std::atomic<int> out{-1};
        auto job = std::make_unique<Job>(Job{trial + 100});
        SHAKE();
        std::thread t = run_job(std::move(job), &out);
        t.join();
        if (out.load() != trial + 100) {
            std::printf("trial %d: out = %d, expected %d\n", trial, out.load(), trial + 100);
            return 1;
        }
    }
    std::printf("20 trials: ownership transferred into the thread correctly\n");
    return 0;
}
