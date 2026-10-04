#include <atomic>
#include <barrier>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

std::vector<long long> run_phased_simulation(
    int n_workers, int n_phases,
    const std::function<long long(int worker, int phase)>& compute,
    const std::function<void(int phase, long long total)>& on_phase_end) {

    if (n_workers <= 0 || n_phases <= 0) return {};

    // One slot per worker, atomic only because ThreadSanitizer cannot see through
    // libc++'s barrier (see section 6). The barrier itself already orders these
    // writes before the completion function reads them.
    std::vector<std::atomic<long long>> slots(static_cast<std::size_t>(n_workers));
    for (auto& s : slots) s.store(0);

    std::vector<long long> totals;
    totals.reserve(static_cast<std::size_t>(n_phases));

    // The completion function. std::barrier runs this on exactly one of the
    // arriving threads, after the last arrival and before ANY thread is released.
    // That makes it the one point in the program where every worker is provably
    // idle, so it can read all the slots at once without a lock — and it is why
    // the phase number can just be however many totals we have collected so far.
    auto aggregate = [&]() noexcept {
        long long total = 0;
        for (auto& s : slots) total += s.load();
        int p = static_cast<int>(totals.size());
        totals.push_back(total);
        if (on_phase_end) on_phase_end(p, total);
    };

    std::barrier sync(n_workers, aggregate);

    std::vector<std::thread> workers;
    for (int w = 0; w < n_workers; ++w) {
        workers.emplace_back([&, w] {
            for (int p = 0; p < n_phases; ++p) {
                slots[static_cast<std::size_t>(w)].store(compute(w, p));
                // Arrive, then block. Nobody is released until everyone has
                // arrived and the aggregation has run, so the next iteration
                // starts a genuinely new phase.
                sync.arrive_and_wait();
            }
        });
    }
    for (auto& t : workers) t.join();

    return totals;
}
