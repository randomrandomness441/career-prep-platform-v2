#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>

// Payload type: counts constructions (atomically, so the test's own bookkeeping
// is race-free) and carries a value written by the constructor, so "did I see a
// fully built object" is checkable, not just "is the pointer non-null".
template <int Round>
struct config {
    inline static std::atomic<long> constructions{0};
    int payload;
    config() : payload(0x1234) { constructions.fetch_add(1, std::memory_order_relaxed); }
};

// Hammer get() from many threads. Threads are staggered by 200us so the
// construction reliably happens once even without synchronization — the point
// is that the *access pattern* is still unsynchronized, which is what the race
// detectors see. Fresh type per round => fresh static state per round.
template <int Round>
static int hammer_once() {
    constexpr int kThreads = 10;
    constexpr int kIters = 1000;

    std::vector<int> rc(kThreads, 0);
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([i, &rc] {
            std::this_thread::sleep_for(std::chrono::microseconds(200 * i));
            SHAKE();
            config<Round>& first = singleton<config<Round>>::get();
            if (first.payload != 0x1234) { rc[i] = 1; return; }
            for (int k = 1; k < kIters; ++k) {
                config<Round>& again = singleton<config<Round>>::get();
                if (&again != &first) { rc[i] = 2; return; }
                if (again.payload != 0x1234) { rc[i] = 3; return; }
                if (k % 64 == 0) SHAKE();
            }
        });
    }
    for (auto& t : threads) t.join();

    for (int i = 0; i < kThreads; ++i) {
        if (rc[i]) {
            std::printf("round %d, thread %d: failed check %d "
                        "(1=unreadable payload, 2=address changed, 3=payload changed)\n",
                        Round, i, rc[i]);
            return 1;
        }
    }
    const long c = config<Round>::constructions.load();
    if (c != 1) {
        std::printf("round %d: constructor ran %ld times, expected exactly 1\n", Round, c);
        return 1;
    }
    return 0;
}

// A type whose constructor throws on the first attempt only.
struct flaky {
    inline static std::atomic<int> attempts{0};
    int value;
    flaky() : value(7) {
        if (attempts.fetch_add(1, std::memory_order_relaxed) == 0)
            throw std::runtime_error("first construction fails");
    }
};

static int throw_retry_check() {
    bool threw = false;
    try {
        (void)singleton<flaky>::get();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!threw) {
        std::printf("flaky: first get() was supposed to throw and did not\n");
        return 1;
    }
    flaky& f = singleton<flaky>::get();  // must attempt construction again
    if (f.value != 7) {
        std::printf("flaky: retried instance has wrong value %d\n", f.value);
        return 1;
    }
    if (flaky::attempts.load() != 2) {
        std::printf("flaky: %d construction attempts, expected 2\n", flaky::attempts.load());
        return 1;
    }
    return 0;
}

int main() {
    if (hammer_once<0>()) return 1;
    if (hammer_once<1>()) return 1;
    if (hammer_once<2>()) return 1;
    if (throw_retry_check()) return 1;

    std::printf("singleton: one construction per round (%ld %ld %ld), throw+retry ok\n",
                config<0>::constructions.load(), config<1>::constructions.load(),
                config<2>::constructions.load());
    return 0;
}
