#include <cstddef>
#include <future>
#include <utility>
#include <vector>

// A correct quicksort, not parallel yet: every recursive call runs on the
// thread that called it.

template <typename T>
std::vector<T> parallel_quick_sort(std::vector<T> input) {
    if (input.size() < 2) return input;

    // Partition around the first element.
    T pivot = std::move(input[0]);
    std::vector<T> lower, upper;
    for (std::size_t i = 1; i < input.size(); ++i) {
        if (input[i] < pivot) lower.push_back(std::move(input[i]));
        else                  upper.push_back(std::move(input[i]));
    }

    // TODO 1: run the lower half on another thread with std::async, and sort
    //         the upper half here while it works.
    // TODO 2: which launch policy? The default one is not what you want.
    // TODO 3: how do you stop the recursion from launching one thread per
    //         element? A 200k-element vector is 200k threads and the thread
    //         constructor will throw.
    std::vector<T> lower_sorted = parallel_quick_sort(std::move(lower));
    std::vector<T> upper_sorted = parallel_quick_sort(std::move(upper));

    lower_sorted.push_back(std::move(pivot));
    for (auto& x : upper_sorted) lower_sorted.push_back(std::move(x));
    return lower_sorted;
}
