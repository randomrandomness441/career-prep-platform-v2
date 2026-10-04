// Harness for the fence version of the store-buffer litmus test. Includes
// the candidate's file verbatim. Same shape as 013's harness -- see that
// question for why this isn't a SHAKE()-style scheduling bug.
#include "solution.hpp"

#include <cstdio>
#include <thread>

int main() {
    const long kTrials = 20000;
    long violations = 0;

    for (long t = 0; t < kTrials; ++t) {
        Handshake h;
        int r1 = -1, r2 = -1;
        std::thread ta([&] { r1 = h.arriveA(); });
        std::thread tb([&] { r2 = h.arriveB(); });
        ta.join();
        tb.join();

        if (r1 == 0 && r2 == 0) ++violations;
    }

    if (violations > 0) {
        std::printf("invariant broken %ld/%ld trials: both arriveA() and arriveB() "
                    "returned 0. The atomics are relaxed by design in this question -- "
                    "the ordering has to come from an atomic_thread_fence placed where "
                    "it actually separates the store from the load on both sides.\n",
                    violations, kTrials);
        return 1;
    }

    std::printf("%ld trials, invariant held every time: at least one side always saw "
                "the other's arrival\n", kTrials);
    return 0;
}
