#include <cstddef>
#include <thread>
#include <vector>

// Exclusive prefix sum (scan): result[i] = sum(input[0..i-1]), result[0] = 0.
//
// Scan looks inherently sequential -- element i's answer depends on every
// element before it. The standard three-pass trick splits it into
// independent local work plus one small, genuinely sequential step:
//
//   1. (parallel) each thread computes the LOCAL exclusive prefix sum of
//      its own chunk, as if that chunk started the array, and records its
//      chunk's total.
//   2. (sequential, cheap) turn the per-chunk totals into per-chunk
//      starting offsets -- this is itself just a prefix sum, but over only
//      num_threads numbers, not n of them.
//   3. (parallel) each thread adds its chunk's starting offset to every
//      value it computed in step 1.
std::vector<long> parallel_prefix_sum(const std::vector<long>& input, int num_threads = 8) {
    const std::size_t n = input.size();
    std::vector<long> result(n);
    if (n == 0) return result;

    num_threads = std::max(1, std::min<int>(num_threads, static_cast<int>(n)));
    std::size_t chunk = (n + num_threads - 1) / num_threads;
    std::vector<long> chunk_total(num_threads, 0);

    // Step 1: local exclusive prefix sums, each thread touching only its
    // own slice of `result` and its own entry in `chunk_total`.
    {
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            std::size_t lo = static_cast<std::size_t>(t) * chunk;
            std::size_t hi = std::min(lo + chunk, n);
            threads.emplace_back([&, t, lo, hi] {
                long running = 0;
                for (std::size_t i = lo; i < hi; ++i) {
                    result[i] = running;
                    running += input[i];
                }
                chunk_total[t] = running;
            });
        }
        for (auto& th : threads) th.join();
    }

    // Step 2: sequential prefix sum over just the num_threads chunk totals
    // -- this is the "carry" every chunk after the first is missing.
    std::vector<long> chunk_offset(num_threads, 0);
    long running = 0;
    for (int t = 0; t < num_threads; ++t) {
        chunk_offset[t] = running;
        running += chunk_total[t];
    }

    // Step 3: apply each chunk's offset to the local sums it already has.
    {
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            std::size_t lo = static_cast<std::size_t>(t) * chunk;
            std::size_t hi = std::min(lo + chunk, n);
            long offset = chunk_offset[t];
            threads.emplace_back([&, lo, hi, offset] {
                for (std::size_t i = lo; i < hi; ++i) result[i] += offset;
            });
        }
        for (auto& th : threads) th.join();
    }

    return result;
}
