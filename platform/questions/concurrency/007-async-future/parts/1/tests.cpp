// Harness for spawn_task. Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

struct Boom : std::runtime_error {
    Boom() : std::runtime_error("boom") {}
};

// A callable that cannot be copied, only moved.
struct MoveOnlyJob {
    std::unique_ptr<int> payload;
    int operator()(int k) const { return *payload * k; }
};

int main() {
    const std::thread::id main_id = std::this_thread::get_id();

    // 1. it returns the value
    {
        std::future<int> f = spawn_task([]{ SHAKE(); return 42; });
        int v = f.get();
        if (v != 42) { std::printf("expected 42, got %d\n", v); return 1; }
    }

    // 2. it runs somewhere else
    {
        std::future<std::thread::id> f =
            spawn_task([]{ SHAKE(); return std::this_thread::get_id(); });
        if (f.get() == main_id) {
            std::printf("the task ran on the calling thread. spawn_task must put it "
                        "on a thread of its own, and must not wait for it.\n");
            return 1;
        }
    }

    // 3. it does not block the caller.
    //    The task parks until main lets it go; if spawn_task ran the task
    //    inline this would never return.
    {
        std::atomic<bool> go{false};
        std::atomic<bool> caller_returned{false};
        std::future<int> f = spawn_task([&]{
            while (!go.load()) std::this_thread::yield();
            return caller_returned.load() ? 1 : 0;
        });
        caller_returned.store(true);
        go.store(true);
        int v = f.get();
        if (v != 1) {
            std::printf("spawn_task did not return until the task had finished\n");
            return 1;
        }
    }

    // 4. arguments are forwarded, and copied before spawn_task returns
    {
        std::future<int> f = spawn_task([](int a, int b){ SHAKE(); return a * b; }, 6, 7);
        if (f.get() != 42) { std::printf("arguments were not forwarded correctly\n"); return 1; }
    }

    // 5. move-only argument
    {
        std::future<int> f = spawn_task(
            [](std::unique_ptr<int> p){ SHAKE(); return *p + 1; }, std::make_unique<int>(7));
        if (f.get() != 8) { std::printf("move-only argument was not moved through\n"); return 1; }
    }

    // 6. move-only callable
    {
        MoveOnlyJob job{std::make_unique<int>(5)};
        std::future<int> f = spawn_task(std::move(job), 4);
        if (f.get() != 20) { std::printf("move-only callable failed\n"); return 1; }
    }

    // 7. void return
    {
        std::atomic<int> hits{0};
        std::future<void> f = spawn_task([&]{ SHAKE(); hits.fetch_add(1); });
        f.get();
        if (hits.load() != 1) { std::printf("void task did not run exactly once\n"); return 1; }
    }

    // 8. an exception from the task must come back through get(), and must be
    //    the original exception -- not future_error/broken_promise, and
    //    certainly not std::terminate.
    {
        std::printf("running the exception-propagation case\n");
        std::fflush(stdout);
        std::future<int> f = spawn_task([]() -> int { SHAKE(); throw Boom{}; });
        try {
            int v = f.get();
            std::printf("the task threw, but get() returned %d\n", v);
            return 1;
        } catch (const Boom&) {
            // correct
        } catch (const std::future_error& e) {
            std::printf("get() threw future_error (%s) instead of the task's own "
                        "exception. The promise was destroyed without ever being "
                        "given a value or an exception.\n", e.what());
            return 1;
        } catch (const std::exception& e) {
            std::printf("get() threw the wrong exception: %s\n", e.what());
            return 1;
        }
    }

    // 9. many tasks in flight at once
    {
        std::vector<std::future<int>> fs;
        for (int i = 0; i < 32; ++i)
            fs.push_back(spawn_task([](int k){ SHAKE(); return k * k; }, i));
        for (int i = 0; i < 32; ++i) {
            int v = fs[static_cast<std::size_t>(i)].get();
            if (v != i * i) {
                std::printf("task %d returned %d, expected %d\n", i, v, i * i);
                return 1;
            }
        }
    }

    std::printf("value, off-thread execution, forwarding, move-only, void, "
                "exception propagation and 32 concurrent tasks all correct\n");
    return 0;
}
