#include <cstddef>
#include <future>
#include <iterator>
#include <thread>
#include <utility>
#include <vector>

namespace pqs {

// How many levels of the recursion tree are allowed to spawn a task.
// Level 0 spawns 1, level 1 spawns 2, ... so `d` levels put ~2^d tasks in
// flight. Sized from the core count, computed once.
inline int max_spawn_depth() {
    static const int d = [] {
        unsigned n = std::thread::hardware_concurrency();
        if (n == 0) n = 2;
        int k = 0;
        while ((1u << k) < n) ++k;
        return k + 1;          // a little oversubscription covers uneven splits
    }();
    return d;
}

// Anything smaller than this is not worth a thread: a thread costs tens of
// microseconds to create, and sorting a few hundred ints costs less than that.
inline constexpr std::size_t kSpawnMin = 1000;

template <typename T>
std::vector<T> sort_impl(std::vector<T> input, int depth);

// Glue the three pieces back together: lower, then the pivot, then upper.
template <typename T>
std::vector<T> stitch(std::vector<T> lower, T pivot, std::vector<T> upper) {
    lower.push_back(std::move(pivot));
    lower.insert(lower.end(),
                 std::make_move_iterator(upper.begin()),
                 std::make_move_iterator(upper.end()));
    return lower;
}

template <typename T>
std::vector<T> sort_impl(std::vector<T> input, int depth) {
    if (input.size() < 2) return input;

    T pivot = std::move(input[0]);
    std::vector<T> lower, upper;
    for (std::size_t i = 1; i < input.size(); ++i) {
        if (input[i] < pivot) lower.push_back(std::move(input[i]));
        else                  upper.push_back(std::move(input[i]));
    }

    if (depth > 0 && input.size() >= kSpawnMin) {
        // std::launch::async is not optional here. The default policy lets the
        // implementation run the task on this very thread, at get() time, and
        // then nothing is parallel at all.
        std::future<std::vector<T>> lower_f = std::async(
            std::launch::async, sort_impl<T>, std::move(lower), depth - 1);

        // Do our own half while the task runs. If sort_impl throws here, the
        // future's destructor waits for the task before the stack unwinds
        // past it -- so the task never outlives the data it is using.
        std::vector<T> upper_sorted = sort_impl(std::move(upper), depth - 1);

        // get() blocks until the task is done and RETHROWS whatever the task
        // threw. Exceptions cross the thread boundary here; they cannot cross
        // a bare std::thread boundary at all.
        std::vector<T> lower_sorted = lower_f.get();

        return stitch(std::move(lower_sorted), std::move(pivot),
                      std::move(upper_sorted));
    }

    std::vector<T> lower_sorted = sort_impl(std::move(lower), depth);
    std::vector<T> upper_sorted = sort_impl(std::move(upper), depth);
    return stitch(std::move(lower_sorted), std::move(pivot),
                  std::move(upper_sorted));
}

}  // namespace pqs

template <typename T>
std::vector<T> parallel_quick_sort(std::vector<T> input) {
    return pqs::sort_impl(std::move(input), pqs::max_spawn_depth());
}
