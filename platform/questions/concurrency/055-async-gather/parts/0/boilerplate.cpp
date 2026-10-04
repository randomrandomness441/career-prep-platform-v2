#include <future>
#include <vector>

// Waits for every future in `futures` and returns their results as a
// vector, in the SAME order the futures were given, not the order the
// underlying tasks happen to finish in.
template <typename T>
std::vector<T> gather(std::vector<std::future<T>> futures) {
    // TODO: implement
    (void)futures;
    return {};
}
