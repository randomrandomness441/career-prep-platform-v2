#include <future>
#include <utility>
#include <vector>

// Waits for every future in `futures` and returns their results as a
// vector -- in the SAME order the futures were given, not the order the
// underlying tasks happen to finish in. If any task threw, the first such
// exception (by input position) propagates out of gather() once every
// future has been waited on.
//
// The whole "trick" here is that there isn't one: std::future::get() on
// futures[0] already blocks until task 0 is done, however long that
// takes, before this loop even looks at futures[1] -- input order and
// "wait for this one specifically" fall out of a plain sequential loop
// for free. No polling, no extra synchronization needed.
template <typename T>
std::vector<T> gather(std::vector<std::future<T>> futures) {
    std::vector<T> results;
    results.reserve(futures.size());
    for (auto& f : futures) {
        results.push_back(f.get());
    }
    return results;
}
