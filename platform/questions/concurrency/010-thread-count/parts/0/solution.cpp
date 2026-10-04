#include <algorithm>
#include <cstddef>
#include <numeric>
#include <thread>
#include <vector>

// Below this much work, a thread costs more than it saves. Measured on this
// machine: launching one thread is ~20-35 us, which is about the time it takes
// one core to add a few hundred thousand integers.
inline constexpr std::size_t min_per_thread = 1000;

inline std::size_t plan_threads(std::size_t length) {
    if (length == 0) return 0;

    // How many threads could this range keep usefully busy?
    const std::size_t max_useful = (length + min_per_thread - 1) / min_per_thread;

    // hardware_concurrency() is a HINT. It is allowed to return 0, meaning
    // "the implementation has no idea". Never divide by it without checking.
    const unsigned hw = std::thread::hardware_concurrency();
    const std::size_t cap = hw != 0 ? static_cast<std::size_t>(hw) : 2;

    return std::min(cap, max_useful);
}

// E is the element type stored in v, T is the accumulator/result type -- kept
// separate because a caller may fold `int` elements into a `long long` total.
// Plain std::vector rather than an arbitrary iterator pair, though: iterator
// genericity was never the point of the exercise, deciding the thread count
// and folding `init` in exactly once are.
template <typename E, typename T>
T parallel_accumulate(const std::vector<E>& v, T init) {
    const std::size_t length = v.size();
    const std::size_t num_threads = plan_threads(length);

    // 0 or 1: not worth a thread. Do it here and skip every bit of overhead.
    if (num_threads <= 1) return std::accumulate(v.begin(), v.end(), init);

    const std::size_t block_size = length / num_threads;

    std::vector<T> results(num_threads);
    std::vector<std::thread> threads;
    threads.reserve(num_threads - 1);

    std::size_t block_start = 0;
    for (std::size_t i = 0; i + 1 < num_threads; ++i) {
        const std::size_t block_end = block_start + block_size;
        // Each worker owns results[i] alone, so the shared vector needs no lock.
        threads.emplace_back([&v, block_start, block_end, &results, i] {
            results[i] = std::accumulate(v.begin() + static_cast<std::ptrdiff_t>(block_start),
                                          v.begin() + static_cast<std::ptrdiff_t>(block_end), T());
        });
        block_start = block_end;
    }

    // The caller is one of the workers: it takes the final block, which also
    // absorbs whatever the integer division left over.
    results[num_threads - 1] =
        std::accumulate(v.begin() + static_cast<std::ptrdiff_t>(block_start), v.end(), T());

    for (auto& t : threads) t.join();

    // init is folded in exactly once, here, after everything has been joined.
    return std::accumulate(results.begin(), results.end(), init);
}
