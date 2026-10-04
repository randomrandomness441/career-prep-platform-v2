#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

// Runs n_workers threads through n_phases phases in lockstep: every worker
// computes its own value for phase p, then nobody moves on to phase p+1
// until all of them have finished phase p, and exactly one of them has
// combined everyone's phase-p values into a total (on_phase_end, if given,
// is told each phase's total as it's computed). Returns every phase's
// total, in order.
std::vector<long long> run_phased_simulation(
    int n_workers, int n_phases,
    const std::function<long long(int worker, int phase)>& compute,
    const std::function<void(int phase, long long total)>& on_phase_end) {

    if (n_workers <= 0 || n_phases <= 0) return {};

    // TODO: implement
    (void)compute;
    (void)on_phase_end;
    return {};
}
