// Harness for run_task (Exceptions Across Thread Boundaries).
// Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <future>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

struct BoomError : std::runtime_error {
    BoomError() : std::runtime_error("boom") {}
};

int main() {
    // ── Part A: an exception thrown inside the task must come back out of
    // future::get() -- not std::terminate() the whole process. The naive
    // version really does call std::terminate() here, so this check runs in
    // a forked child: fork() before any threads exist, so only the child can
    // be killed, and this test process survives to report the result either
    // way.
    {
        pid_t pid = fork();
        if (pid < 0) { std::perror("fork"); return 1; }

        if (pid == 0) {
            // child process
            try {
                std::future<int> f = run_task([]() -> int { throw BoomError{}; });
                int v = f.get();
                std::fprintf(stderr, "run_task returned %d instead of rethrowing\n", v);
                _exit(1);
            } catch (const BoomError&) {
                _exit(0);   // exactly the exception that was thrown, via get()
            } catch (const std::exception& e) {
                std::fprintf(stderr, "wrong exception out of get(): %s\n", e.what());
                _exit(2);
            }
        }

        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFSIGNALED(status)) {
            std::printf(
                "run_task let the task's exception escape the worker thread: the "
                "process was killed by signal %d (%s), consistent with "
                "std::terminate(). Catch the exception on the worker thread and "
                "hand it to the promise with set_exception(std::current_exception()) "
                "instead of letting it propagate out of the thread's entry function.\n",
                WTERMSIG(status), strsignal(WTERMSIG(status)));
            return 1;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::printf("exception-propagation check failed (child exit code %d)\n",
                        WIFEXITED(status) ? WEXITSTATUS(status) : -1);
            return 1;
        }
    }

    // ── Part B: the non-throwing path still has to actually work.
    {
        std::future<int> f = run_task([] { return 42; });
        SHAKE();
        int v = f.get();
        if (v != 42) { std::printf("run_task returned %d, expected 42\n", v); return 1; }
    }

    // ── Part C: void-returning tasks -- set_value() takes no argument here,
    // so this exercises the if constexpr branch specifically.
    {
        std::atomic<bool> ran{false};
        std::future<void> f = run_task([&] { ran.store(true); });
        f.get();
        if (!ran.load()) { std::printf("void task's body never ran\n"); return 1; }
    }

    // ── Part D: many tasks in flight together, some throwing, some not --
    // each future must carry only its own task's outcome.
    {
        constexpr int kTasks = 20;
        std::vector<std::future<int>> oks;
        std::vector<std::future<int>> bads;
        for (int i = 0; i < kTasks; ++i) {
            oks.push_back(run_task([i] { return i * i; }));
            bads.push_back(run_task([]() -> int { throw BoomError{}; }));
        }
        for (int i = 0; i < kTasks; ++i) {
            SHAKE();
            int got = oks[i].get();
            if (got != i * i) {
                std::printf("task %d: got %d, expected %d\n", i, got, i * i);
                return 1;
            }
            bool caught = false;
            try {
                bads[i].get();
            } catch (const BoomError&) {
                caught = true;
            }
            if (!caught) {
                std::printf("task %d: expected exception was not delivered\n", i);
                return 1;
            }
        }
    }

    std::printf("exceptions come back through future::get() instead of terminating, "
                "non-throwing and void tasks work, and %d concurrent tasks don't "
                "cross-talk\n",
                20);
    return 0;
}
