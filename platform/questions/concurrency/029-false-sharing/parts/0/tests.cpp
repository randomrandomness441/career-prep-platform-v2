// Harness for CounterBank<N> (False Sharing). Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <cstdint>
#include <thread>
#include <vector>

static constexpr std::size_t kN = 8;
static constexpr long kIncrements = 200;
static constexpr std::size_t kCacheLine = 64;

int main() {
    CounterBank<kN> bank;

    // ── Part A: layout. This is the actual point of the exercise. A cache
    // line is 64 bytes on every machine this course targets; two counters
    // whose addresses fall in the same 64-byte-aligned block share a line,
    // and every write to one invalidates the other core's copy of it. This
    // check is deterministic -- it depends only on where the compiler and
    // linker placed the object, not on scheduling, so it either fails every
    // run or passes every run.
    {
        std::uintptr_t line[kN];
        for (std::size_t i = 0; i < kN; ++i) {
            line[i] = bank.address_of(i) / kCacheLine;
        }
        for (std::size_t i = 0; i < kN; ++i) {
            for (std::size_t j = i + 1; j < kN; ++j) {
                if (line[i] == line[j]) {
                    std::printf("counters %zu and %zu share a 64-byte cache line "
                                "(addresses 0x%lx and 0x%lx) -- a write to one will "
                                "invalidate the other core's copy of the whole line, "
                                "even though the two counters have nothing to do with "
                                "each other. Pad each counter to its own cache line.\n",
                                i, j, (unsigned long)bank.address_of(i),
                                (unsigned long)bank.address_of(j));
                    return 1;
                }
            }
        }
    }

    // ── Part B: it still has to work. N threads, each owning exactly one
    // counter, incrementing it kIncrements times. Nothing here should race --
    // padding does not change correctness -- but a broken padding scheme
    // (e.g. one that overlaps storage) could break it, so we check for real.
    {
        std::vector<std::thread> threads;
        for (std::size_t i = 0; i < kN; ++i) {
            threads.emplace_back([&bank, i] {
                for (long k = 0; k < kIncrements; ++k) {
                    SHAKE();
                    bank.increment(i);
                }
            });
        }
        for (auto& t : threads) t.join();

        for (std::size_t i = 0; i < kN; ++i) {
            if (bank.get(i) != kIncrements) {
                std::printf("counter %zu = %ld, expected %ld\n", i, bank.get(i), kIncrements);
                return 1;
            }
        }
    }

    std::printf("%zu counters, each on its own cache line, each counted exactly right\n", kN);
    return 0;
}
