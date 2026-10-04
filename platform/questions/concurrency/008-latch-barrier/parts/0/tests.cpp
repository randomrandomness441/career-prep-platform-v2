#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <functional>
#include <vector>

namespace {

// Every worker publishes how many phases it has completed. The barrier's whole
// promise is that when worker w is inside phase p, every other worker v has
// completed either p phases (it is in phase p too, or about to enter it) or p+1
// (it already finished phase p and is waiting). Anything else means somebody
// crossed a phase boundary that was not open yet.
struct Watch {
    std::vector<std::atomic<int>> finished;
    std::atomic<int> claimed{0};
    std::atomic<int> v_worker{-1}, v_phase{-1}, v_other{-1}, v_other_done{-1};
    std::atomic<int> in_compute{0};
    std::atomic<int> aggregated_while_busy{0};
    std::atomic<int> aggregations{0};
    std::atomic<int> bad_total{0};
    int n;

    explicit Watch(int workers) : finished(static_cast<std::size_t>(workers)), n(workers) {
        for (auto& f : finished) f.store(0);
    }

    void record(int w, int p, int other, int done) {
        int expect = 0;
        if (claimed.compare_exchange_strong(expect, 1)) {
            v_worker.store(w);
            v_phase.store(p);
            v_other.store(other);
            v_other_done.store(done);
            claimed.store(2);
        }
    }

    void check_all_in_step(int w, int p) {
        int workers = n;
        for (int v = 0; v < workers; ++v) {
            int done = finished[static_cast<std::size_t>(v)].load(std::memory_order_acquire);
            if (done < p || done > p + 1) record(w, p, v, done);
        }
    }
};

long long expected_total(int n_workers, int phase) {
    // contribution of worker w in phase p is (w+1) * (p+1)
    long long sum = 0;
    for (int w = 0; w < n_workers; ++w) sum += static_cast<long long>(w + 1) * (phase + 1);
    return sum;
}

bool run_case(int n_workers, int n_phases) {
    Watch watch(n_workers);

    auto compute = [&](int w, int p) -> long long {
        watch.in_compute.fetch_add(1, std::memory_order_acq_rel);
        watch.check_all_in_step(w, p);
        SHAKE();
        long long contribution = static_cast<long long>(w + 1) * (p + 1);
        watch.check_all_in_step(w, p);
        watch.in_compute.fetch_sub(1, std::memory_order_acq_rel);
        // Published last: after this line the rest of the world may consider
        // this worker done with phase p.
        watch.finished[static_cast<std::size_t>(w)].fetch_add(1, std::memory_order_release);
        return contribution;
    };

    auto on_phase_end = [&](int p, long long total) {
        if (watch.in_compute.load(std::memory_order_acquire) != 0)
            watch.aggregated_while_busy.store(1);
        watch.aggregations.fetch_add(1);
        if (total != expected_total(n_workers, p)) watch.bad_total.store(1);
    };

    std::vector<long long> totals =
        run_phased_simulation(n_workers, n_phases, compute, on_phase_end);

    if (watch.claimed.load() != 0) {
        std::printf("phase boundary crossed early: worker %d was inside phase %d while "
                    "worker %d had completed %d phases (legal: %d or %d)\n",
                    watch.v_worker.load(), watch.v_phase.load(), watch.v_other.load(),
                    watch.v_other_done.load(), watch.v_phase.load(), watch.v_phase.load() + 1);
        return false;
    }
    if (watch.aggregated_while_busy.load() != 0) {
        std::printf("the phase aggregation ran while a worker was still inside "
                    "compute() - it did not see a settled phase\n");
        return false;
    }
    if (watch.aggregations.load() != n_phases) {
        std::printf("aggregation ran %d times for %d phases — it must run exactly "
                    "once between phases\n", watch.aggregations.load(), n_phases);
        return false;
    }
    if (watch.bad_total.load() != 0) {
        std::printf("a phase total did not match the sum of that phase's "
                    "contributions\n");
        return false;
    }
    if (static_cast<int>(totals.size()) != n_phases) {
        std::printf("returned %d totals, expected %d\n",
                    static_cast<int>(totals.size()), n_phases);
        return false;
    }
    for (int p = 0; p < n_phases; ++p) {
        if (totals[static_cast<std::size_t>(p)] != expected_total(n_workers, p)) {
            std::printf("totals[%d] = %lld, expected %lld\n", p,
                        totals[static_cast<std::size_t>(p)], expected_total(n_workers, p));
            return false;
        }
    }
    for (int w = 0; w < n_workers; ++w) {
        int done = watch.finished[static_cast<std::size_t>(w)].load();
        if (done != n_phases) {
            std::printf("worker %d ran %d phases, expected %d\n", w, done, n_phases);
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    struct Case { int workers; int phases; };
    const Case cases[] = {
        {4, 40}, {2, 60}, {8, 12}, {3, 25}, {1, 8}, {6, 20},
    };

    for (int trial = 0; trial < 3; ++trial) {
        for (const Case& c : cases) {
            if (!run_case(c.workers, c.phases)) {
                std::printf("   (failed with %d workers, %d phases, trial %d)\n",
                            c.workers, c.phases, trial);
                return 1;
            }
        }
    }

    // Degenerate inputs must not hang or crash.
    if (!run_phased_simulation(0, 5, [](int, int) { return 1LL; },
                               [](int, long long) {}).empty()) {
        std::printf("zero workers should produce no phases\n");
        return 1;
    }
    if (!run_phased_simulation(4, 0, [](int, int) { return 1LL; },
                               [](int, long long) {}).empty()) {
        std::printf("zero phases should produce no totals\n");
        return 1;
    }

    std::printf("phases stayed in lockstep, aggregation ran once per phase with "
                "every worker idle, totals correct\n");
    return 0;
}
