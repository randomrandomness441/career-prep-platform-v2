#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

// Argsort for a large array with a SMALL number of distinct values -- the
// shape where counting sort beats a comparison sort. Splits the array into
// one chunk per thread; each thread counts its own chunk into a LOCAL
// histogram (no sharing, so no synchronization needed for this part at
// all); the local histograms are merged sequentially (cheap: num_buckets is
// small); a prefix sum turns the merged histogram into starting offsets per
// bucket; a second parallel pass scatters each element to its final
// position via one atomic increment per bucket.
std::vector<std::size_t> parallel_argsort(const std::vector<int>& values, int num_buckets,
                                           int num_threads = 8) {
    const std::size_t n = values.size();
    if (n == 0) return {};
    num_threads = std::max(1, std::min<int>(num_threads, static_cast<int>(n)));
    std::size_t chunk = (n + num_threads - 1) / num_threads;

    // Pass 1: local histograms, one per thread, no shared state at all.
    std::vector<std::vector<std::size_t>> local_counts(
        num_threads, std::vector<std::size_t>(num_buckets, 0));
    {
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            std::size_t lo = static_cast<std::size_t>(t) * chunk;
            std::size_t hi = std::min(lo + chunk, n);
            threads.emplace_back([&, t, lo, hi] {
                for (std::size_t i = lo; i < hi; ++i) {
                    ++local_counts[t][static_cast<std::size_t>(values[i])];
                }
            });
        }
        for (auto& th : threads) th.join();
    }

    // Merge: cheap, sequential -- num_threads * num_buckets additions.
    std::vector<std::size_t> merged(num_buckets, 0);
    for (int t = 0; t < num_threads; ++t)
        for (int b = 0; b < num_buckets; ++b) merged[b] += local_counts[t][b];

    // Exclusive prefix sum -> starting offset of each bucket in the output.
    std::vector<std::atomic<std::size_t>> cursor(num_buckets);
    std::size_t running = 0;
    for (int b = 0; b < num_buckets; ++b) {
        cursor[b].store(running, std::memory_order_relaxed);
        running += merged[b];
    }

    // Pass 2: scatter. Each element's final index is claimed with one
    // atomic fetch_add on its bucket's cursor -- correct regardless of
    // which thread gets there first, at the cost of one atomic op per
    // element (num_buckets separate cursors keeps that contention spread
    // out rather than funneled through a single counter).
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
                    perm[pos] = i;
                }
            });
        }
        for (auto& th : threads) th.join();
    }
    return perm;
}
