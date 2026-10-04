// Harness for the store-buffer litmus test. Includes the candidate's file verbatim.
//
// This is not a scheduling-interleaving bug (concur/stress.hpp's SHAKE() has
// nothing to inject here) -- it's a genuine CPU/compiler memory-reordering
// effect. The signal is a fresh Handshake and a fresh pair of threads on
// every trial, many trials, counting how often the invariant breaks.
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
                    "returned 0 -- each side announced itself and then failed to see "
                    "the other, even though one write necessarily happened before the "
                    "other side's read. That's the memory order being too weak, not a "
                    "logic error in the handshake itself.\n",
                    violations, kTrials);
        return 1;
    }

    std::printf("%ld trials, invariant held every time: at least one side always saw "
                "the other's arrival\n", kTrials);
    return 0;
}
