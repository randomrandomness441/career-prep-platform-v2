#include <cstddef>
#include <thread>
#include <vector>

// Exclusive prefix sum (scan): result[i] = sum(input[0..i-1]), result[0] = 0.
// Split the work across num_threads threads.
std::vector<long> parallel_prefix_sum(const std::vector<long>& input, int num_threads = 8) {
    const std::size_t n = input.size();
    std::vector<long> result(n);
    if (n == 0) return result;

    // TODO: implement
    return result;
}
