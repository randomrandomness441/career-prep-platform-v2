// Harness for "Work-Stealing Thread Pool". Includes the candidate's file
// verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

int main() {
    // ── 1. the destructor must not crash the process. Runs in a forked
    // child: fork() before any threads exist, so only the child can be
    // killed by a joinable std::thread's destructor calling
    // std::terminate(), and this process survives to report the result
    // either way.
    {
        pid_t pid = fork();
        if (pid < 0) { std::perror("fork"); return 1; }

        if (pid == 0) {
            {
                WorkStealingPool pool(8);
                for (int i = 0; i < 500; ++i) pool.submit([] {});
            }  // destructor runs here, immediately
            _exit(0);
        }

        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFSIGNALED(status)) {
            std::printf(
                "the pool's destructor let a still-running std::thread be destroyed: the "
                "process was killed by signal %d (%s), consistent with std::terminate() "
                "from a joinable thread's destructor. Join every worker before the "
                "destructor returns.\n",
                WTERMSIG(status), strsignal(WTERMSIG(status)));
            return 1;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::printf("destructor check failed (child exit status %d)\n",
                        WIFEXITED(status) ? WEXITSTATUS(status) : -1);
            return 1;
        }
    }

    // ── 2. every submitted task actually runs before the pool is
    // destroyed, including tasks submitted right up until shutdown.
    {
        constexpr int kWorkers = 8;
        constexpr int kTasks = 4000;
        std::atomic<int> ran{0};
        {
            WorkStealingPool pool(kWorkers);
            for (int i = 0; i < kTasks; ++i) {
                SHAKE();
                pool.submit([&ran] { ran.fetch_add(1, std::memory_order_relaxed); });
            }
        }
        if (ran.load() != kTasks) {
            std::printf("%d of %d submitted tasks ran before the pool finished shutting "
                        "down, expected all %d\n", ran.load(), kTasks, kTasks);
            return 1;
        }
    }

    // ── 3. work distributes across more than one worker (this pool is
    // pointless if everything piles onto one queue and nothing gets
    // stolen or round-robined elsewhere).
    {
        std::atomic<long> seen_mask{0};
        std::atomic<int> distinct{0};
        std::atomic<int> done{0};
        WorkStealingPool pool(8);
        for (int i = 0; i < 500; ++i) {
            pool.submit([&] {
                long bit = 1L << (std::hash<std::thread::id>{}(std::this_thread::get_id()) % 63);
                if ((seen_mask.fetch_or(bit, std::memory_order_relaxed) & bit) == 0)
                    distinct.fetch_add(1, std::memory_order_relaxed);
                done.fetch_add(1, std::memory_order_relaxed);
            });
        }
        while (done.load(std::memory_order_relaxed) < 500) std::this_thread::yield();
        if (distinct.load() < 2) {
            std::printf("500 tasks ran on only %d distinct worker thread(s)\n", distinct.load());
            return 1;
        }
    }

    std::printf("destructor doesn't crash, %d submitted tasks all ran before shutdown, "
                "and work distributes across multiple workers\n", 4000);
    return 0;
}
