// Harness for "Ordered Disk Log". Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

int main() {
    // Many threads writing concurrently; each records what ticket it was
    // given and what it wrote. The log's final contents must have entry k
    // equal to whichever record was assigned ticket k -- regardless of
    // which thread's write physically reached the log first.
    constexpr int kThreads = 16;
    constexpr int kPerThread = 500;
    constexpr int kTotal = kThreads * kPerThread;

    OrderedLog log;
    std::vector<std::string> expected(kTotal);
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < kPerThread; ++i) {
                std::string rec = "t" + std::to_string(t) + "_" + std::to_string(i);
                SHAKE();
                std::uint64_t ticket = log.write_record(rec);
                if (ticket >= static_cast<std::uint64_t>(kTotal)) {
                    std::printf("ticket %llu out of range (expected < %d)\n",
                                (unsigned long long)ticket, kTotal);
                    std::exit(1);
                }
                expected[ticket] = rec;
            }
        });
    }
    for (auto& th : threads) th.join();

    std::vector<std::string> got = log.contents();
    if (got.size() != static_cast<std::size_t>(kTotal)) {
        std::printf("log has %zu entries, expected %d\n", got.size(), kTotal);
        return 1;
    }
    for (int i = 0; i < kTotal; ++i) {
        if (got[static_cast<std::size_t>(i)] != expected[static_cast<std::size_t>(i)]) {
            std::printf("log entry %d is \"%s\", expected \"%s\" (the record that was "
                        "assigned ticket %d) -- the log is not in ticket order\n",
                        i, got[static_cast<std::size_t>(i)].c_str(),
                        expected[static_cast<std::size_t>(i)].c_str(), i);
            return 1;
        }
    }

    std::printf("%d threads x %d writes each: log is in exact ticket order\n",
                kThreads, kPerThread);
    return 0;
}
