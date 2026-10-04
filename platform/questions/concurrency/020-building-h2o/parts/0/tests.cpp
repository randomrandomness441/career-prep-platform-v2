// Harness for "Building H2O".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

const int kTrials    = 10;
const int kMolecules = 20;   // 40 hydrogen threads + 20 oxygen threads per trial

// The output tape. Guarded by its own mutex, so the tape itself is never the
// race being measured — only the ORDER of what lands on it is under test.
struct Tape {
    std::mutex m;
    std::string atoms;

    void emit(char c) {
        std::lock_guard<std::mutex> lk(m);
        atoms.push_back(c);
    }
};

}  // namespace

int main() {
    for (int trial = 0; trial < kTrials; ++trial) {
        H2O plant;
        Tape tape;
        std::vector<std::thread> threads;
        threads.reserve(static_cast<std::size_t>(3 * kMolecules));

        // Deliberately lopsided arrival: every hydrogen shows up before the first
        // oxygen. Nothing about the problem forbids this, and an implementation
        // that only works when arrivals interleave nicely is not a solution.
        for (int i = 0; i < 2 * kMolecules; ++i)
            threads.emplace_back([&] {
                plant.hydrogen([&] { SHAKE(); tape.emit('H'); });
            });
        for (int i = 0; i < kMolecules; ++i)
            threads.emplace_back([&] {
                plant.oxygen([&] { SHAKE(); tape.emit('O'); });
            });

        for (std::thread& t : threads) t.join();

        // Nobody may be left holding an atom.
        if (tape.atoms.size() != static_cast<std::size_t>(3 * kMolecules)) {
            std::printf("trial %d: expected %d atoms, got %zu\n",
                        trial, 3 * kMolecules, tape.atoms.size());
            return 1;
        }

        // Every consecutive group of three must be exactly 2 H and 1 O.
        for (int g = 0; g < kMolecules; ++g) {
            const std::string group = tape.atoms.substr(static_cast<std::size_t>(3 * g), 3);
            int h = 0, o = 0;
            for (const char c : group) {
                if (c == 'H') ++h;
                else if (c == 'O') ++o;
            }
            if (h != 2 || o != 1) {
                std::printf("trial %d: group %d of the output is \"%s\" — %d hydrogen and "
                            "%d oxygen, not 2 and 1.\nOutput around it: ...%s...\n",
                            trial, g, group.c_str(), h, o,
                            tape.atoms.substr(static_cast<std::size_t>(g >= 2 ? 3 * (g - 2) : 0),
                                              15).c_str());
                return 1;
            }
        }
    }

    std::printf("%d trials x %d molecules: every group of three is exactly 2 H and 1 O\n",
                kTrials, kMolecules);
    return 0;
}
