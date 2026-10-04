#include <cstddef>
#include <numeric>
#include <thread>
#include <vector>

// How many threads should a job of this size get? Think about both the
// machine (how many cores are actually available, and what
// hardware_concurrency() is and isn't allowed to return) and the job (how
// much work is really there to hand off -- creating a thread costs real
// time, so a tiny job shouldn't get one at all).
inline std::size_t plan_threads(std::size_t length) {
    // TODO: implement
    return 1;
}

// E is the element type stored in v, T is the accumulator/result type.
// Sum v into T, starting from init, splitting the work across however many
// threads plan_threads(v.size()) says to use.
template <typename E, typename T>
T parallel_accumulate(const std::vector<E>& v, T init) {
    // TODO: implement
    return init;
}
