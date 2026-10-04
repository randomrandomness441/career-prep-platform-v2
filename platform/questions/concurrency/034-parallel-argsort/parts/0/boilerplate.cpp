#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

// Argsort for a large array with a SMALL number of distinct values, the
// shape where counting sort beats a comparison sort. Count how many times
// each bucket value occurs, turn that into starting offsets via a prefix
// sum, then scatter each element to its final position.
std::vector<std::size_t> parallel_argsort(const std::vector<int>& values, int num_buckets,
                                           int num_threads = 8) {
    const std::size_t n = values.size();
    if (n == 0) return {};
    num_threads = std::max(1, std::min<int>(num_threads, static_cast<int>(n)));
    std::size_t chunk = (n + num_threads - 1) / num_threads;

    // TODO: count how many times each bucket value (0..num_buckets) occurs
    //       across `values`, splitting the work across num_threads threads.
    std::vector<std::size_t> count(num_buckets, 0);

    // Exclusive prefix sum -> starting offset of each bucket in the output.
    std::vector<std::atomic<std::size_t>> cursor(num_buckets);
    std::size_t running = 0;
    for (int b = 0; b < num_buckets; ++b) {
        cursor[b].store(running, std::memory_order_relaxed);
        running += count[b];
    }

    // Scatter: each element's final index is claimed with one atomic
    // fetch_add on its bucket's cursor.
    std::vector<std::size_t> perm(n);
    {
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            std::size_t lo = static_cast<std::size_t>(t) * chunk;
            std::size_t hi = std::min(lo + chunk, n);
            threads.emplace_back([&, lo, hi] {
                for (std::size_t i = lo; i < hi; ++i) {
                    std::size_t b = static_cast<std::size_t>(values[i]);
                    std::size_t pos = cursor[b].fetch_add(1, std::memory_order_relaxed);
                    if (pos < perm.size()) perm[pos] = i;
                }
            });
        }
        for (auto& th : threads) th.join();
    }
    return perm;
}
