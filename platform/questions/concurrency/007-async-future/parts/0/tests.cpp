// Harness for Parallel Quicksort. Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <exception>
#include <random>
#include <thread>
#include <vector>

// ── Probe: an int that remembers which threads compared it ──────────────────
// Every thread that performs at least one comparison bumps the counter once.
// A sort that never leaves the calling thread ends with exactly 1.
static std::atomic<int> g_comparing_threads{0};

struct Probe {
    int v;
};

static bool operator<(const Probe& a, const Probe& b) {
    static thread_local bool counted = false;
    if (!counted) {
        counted = true;
        g_comparing_threads.fetch_add(1, std::memory_order_relaxed);
    }
    return a.v < b.v;
}

// ── Bomb: an int whose comparison throws, but only on a worker thread ───────
// This forces the exception to originate inside an async task, so the only
// way it can reach main() is through future::get().
static std::thread::id g_main_id;

struct BombWentOff : std::exception {
    const char* what() const noexcept override { return "bomb"; }
};

struct Bomb {
    int v;
};

static bool operator<(const Bomb& a, const Bomb& b) {
    if (std::this_thread::get_id() != g_main_id) throw BombWentOff{};
    return a.v < b.v;
}

// ── helpers ─────────────────────────────────────────────────────────────────
static bool check_ints(const char* label, std::vector<int> in) {
    std::vector<int> want = in;
    std::sort(want.begin(), want.end());
    std::vector<int> got = parallel_quick_sort(in);
    if (got == want) return true;
    std::printf("%s: wrong result. got %zu elements, expected %zu",
                label, got.size(), want.size());
    if (got.size() == want.size()) {
        for (std::size_t i = 0; i < got.size(); ++i)
            if (got[i] != want[i]) {
                std::printf("; first difference at index %zu: got %d, expected %d",
                            i, got[i], want[i]);
                break;
            }
    }
    std::printf("\n");
    return false;
}

int main() {
    g_main_id = std::this_thread::get_id();
    std::mt19937 rng(20260907u);

    // 1. degenerate sizes
    for (int n = 0; n < 4; ++n) {
        std::vector<int> v;
        for (int i = 0; i < n; ++i) v.push_back(n - i);
        if (!check_ints("tiny input", v)) return 1;
    }

    // 2. random data, several shapes, several trials
    for (int trial = 0; trial < 4; ++trial) {
        std::vector<int> v(20000);
        for (auto& x : v) x = static_cast<int>(rng() % 1000000u);
        SHAKE();
        if (!check_ints("random", v)) return 1;
    }

    // 3. inputs that break a careless partition
    {
        std::vector<int> asc(1000), desc(1000), same(1000, 42), few(1000);
        for (int i = 0; i < 1000; ++i) { asc[i] = i; desc[i] = 1000 - i; few[i] = i % 3; }
        if (!check_ints("already sorted", asc))  return 1;
        if (!check_ints("reverse sorted", desc)) return 1;
        if (!check_ints("all equal", same))      return 1;
        if (!check_ints("three distinct values", few)) return 1;
        std::vector<int> neg(2000);
        for (auto& x : neg) x = static_cast<int>(rng() % 200u) - 100;
        if (!check_ints("negatives and duplicates", neg)) return 1;
    }

    // 4. it must actually run on more than one thread
    {
        g_comparing_threads.store(0, std::memory_order_relaxed);
        std::vector<Probe> v(60000);
        for (auto& p : v) p.v = static_cast<int>(rng() % 1000000u);
        SHAKE();
        std::vector<Probe> got = parallel_quick_sort(v);
        if (!std::is_sorted(got.begin(), got.end())) {
            std::printf("thread-count probe: result was not sorted\n");
            return 1;
        }
        int threads = g_comparing_threads.load(std::memory_order_relaxed);
        if (threads < 2) {
            std::printf("60000 elements were sorted entirely on the calling thread "
                        "(%d thread did any comparing). Nothing ran in parallel: "
                        "either no std::async call was made, or the launch policy "
                        "let it run deferred on this thread.\n", threads);
            return 1;
        }
    }

    // 5. an exception thrown inside a task must come back out of the sort
    {
        std::vector<Bomb> v(20000);
        for (auto& b : v) b.v = static_cast<int>(rng() % 1000000u);
        bool caught = false;
        try {
            std::vector<Bomb> got = parallel_quick_sort(v);
            std::printf("comparing Bombs on a worker thread throws, but "
                        "parallel_quick_sort returned normally with %zu elements. "
                        "The exception was swallowed.\n", got.size());
            return 1;
        } catch (const BombWentOff&) {
            caught = true;
        } catch (const std::exception& e) {
            std::printf("expected BombWentOff out of the sort, got: %s\n", e.what());
            return 1;
        }
        if (!caught) { std::printf("no exception escaped the sort\n"); return 1; }
    }

    // 6. the caller's vector must not be mutated (it is taken by value)
    {
        std::vector<int> v(5000);
        for (auto& x : v) x = static_cast<int>(rng() % 1000u);
        std::vector<int> before = v;
        std::vector<int> got = parallel_quick_sort(v);
        if (v != before) {
            std::printf("parallel_quick_sort modified the caller's vector\n");
            return 1;
        }
        if (!std::is_sorted(got.begin(), got.end())) {
            std::printf("result not sorted\n");
            return 1;
        }
    }

    std::printf("sorted correctly on every shape, ran on %d threads, "
                "and propagated the task's exception\n",
                g_comparing_threads.load(std::memory_order_relaxed));
    return 0;
}
