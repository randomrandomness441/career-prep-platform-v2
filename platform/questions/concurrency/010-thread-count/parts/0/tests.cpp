#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <mutex>
#include <numeric>
#include <set>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// A value type that records which thread performed each addition. This is how
// the test can tell "did you actually create threads?" from "did you claim to".
// ---------------------------------------------------------------------------
namespace {

std::mutex g_ids_mutex;
std::set<std::thread::id> g_ids;

void note_thread() {
    std::lock_guard<std::mutex> lk(g_ids_mutex);
    g_ids.insert(std::this_thread::get_id());
}

std::size_t distinct_threads() {
    std::lock_guard<std::mutex> lk(g_ids_mutex);
    return g_ids.size();
}

void reset_threads() {
    std::lock_guard<std::mutex> lk(g_ids_mutex);
    g_ids.clear();
}

struct Counted {
    long long v = 0;
    Counted() = default;
    explicit Counted(long long x) : v(x) {}
};

Counted operator+(Counted a, Counted b) {
    note_thread();
    return Counted(a.v + b.v);
}

std::size_t expected_plan(std::size_t n) {
    if (n == 0) return 0;
    const unsigned hw = std::thread::hardware_concurrency();
    const std::size_t cap = hw != 0 ? static_cast<std::size_t>(hw) : 2;
    const std::size_t useful = (n + 999) / 1000;
    return cap < useful ? cap : useful;
}

}  // namespace

int main() {
    // ---- 1. plan_threads follows the contract -----------------------------
    const std::size_t lengths[] = {0, 1, 2, 500, 999, 1000, 1001, 2000, 2001,
                                   50000, 1000000, 100000000};
    for (std::size_t n : lengths) {
        const std::size_t got = plan_threads(n);
        const std::size_t want = expected_plan(n);
        if (got != want) {
            std::printf("plan_threads(%zu) = %zu, expected %zu\n", n, got, want);
            std::printf("  (min_per_thread is 1000; the cap is hardware_concurrency(), "
                        "or 2 if that returns 0)\n");
            return 1;
        }
    }

    // ---- 2. the sum is right, for every awkward length --------------------
    for (int trial = 0; trial < 100; ++trial) {
        const std::size_t sizes[] = {0, 1, 2, 3, 999, 1000, 1001, 2501, 9999, 40000};
        for (std::size_t n : sizes) {
            std::vector<int> v(n);
            for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<int>(i % 7) - 3;

            const long long init = 12345;
            const long long want = std::accumulate(v.begin(), v.end(), init);
            const long long got = parallel_accumulate(v, init);
            if (got != want) {
                std::printf("parallel_accumulate over %zu elements with init=%lld "
                            "returned %lld, expected %lld\n", n, init, got, want);
                if (n == 0) std::printf("  an empty range must return init unchanged\n");
                else if (got == want - init) std::printf("  init was dropped\n");
                return 1;
            }
        }
        SHAKE();
    }

    // ---- 3. a small range must not create ANY thread ----------------------
    {
        for (std::size_t n : {std::size_t{0}, std::size_t{1}, std::size_t{500},
                              std::size_t{1000}}) {
            reset_threads();
            std::vector<Counted> v;
            v.reserve(n);
            for (std::size_t i = 0; i < n; ++i) v.push_back(Counted(1));

            const Counted got = parallel_accumulate(v, Counted(7));
            if (got.v != static_cast<long long>(n) + 7) {
                std::printf("small range of %zu: got %lld, expected %zu\n",
                            n, got.v, n + 7);
                return 1;
            }
            if (distinct_threads() > 1) {
                std::printf("a range of %zu elements was summed by %zu threads -- "
                            "below min_per_thread it must all happen on the caller\n",
                            n, distinct_threads());
                return 1;
            }
        }
    }

    // ---- 4. a big range uses threads, but never more than the cap ---------
    {
        const unsigned hw = std::thread::hardware_concurrency();
        const std::size_t cap = hw != 0 ? static_cast<std::size_t>(hw) : 2;
        const std::size_t n = 200000;

        for (int trial = 0; trial < 3; ++trial) {
            reset_threads();
            std::vector<Counted> v;
            v.reserve(n);
            for (std::size_t i = 0; i < n; ++i) v.push_back(Counted(2));

            const Counted got = parallel_accumulate(v, Counted(5));
            if (got.v != static_cast<long long>(n) * 2 + 5) {
                std::printf("big range: got %lld, expected %lld\n",
                            got.v, static_cast<long long>(n) * 2 + 5);
                return 1;
            }
            const std::size_t used = distinct_threads();
            if (used > cap) {
                std::printf("%zu elements were spread over %zu threads, but the machine "
                            "reports %zu hardware threads -- that is oversubscription\n",
                            n, used, cap);
                return 1;
            }
            if (cap > 1 && used < 2) {
                std::printf("%zu elements were summed on a single thread; "
                            "plan_threads said %zu\n", n, plan_threads(n));
                return 1;
            }
        }
    }

    // ---- 5. several callers at once -- nothing shared between calls -------
    {
        std::vector<int> v(30000, 3);
        std::vector<std::thread> callers;
        std::vector<long long> out(6, 0);
        for (int i = 0; i < 6; ++i) {
            callers.emplace_back([&v, &out, i] {
                SHAKE();
                out[static_cast<std::size_t>(i)] =
                    parallel_accumulate(v, 1LL);
            });
        }
        for (auto& t : callers) t.join();
        for (int i = 0; i < 6; ++i) {
            if (out[static_cast<std::size_t>(i)] != 90001) {
                std::printf("concurrent caller %d got %lld, expected 90001\n",
                            i, out[static_cast<std::size_t>(i)]);
                return 1;
            }
        }
    }

    std::printf("plan honours hardware_concurrency and the work threshold; "
                "sums correct for every length\n");
    return 0;
}
