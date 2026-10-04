// Harness for "Losing the Element" — exception safety of pop().
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

// A type whose copies and moves fail on demand.
//
// `budget` is how many more copy/move operations will succeed. The one after
// that throws. Setting budget to 1 means "let the first construction through,
// then break" — which is exactly the window where a naive pop() has already
// removed the element from the container but has not yet handed it over.
struct Fragile {
    static std::atomic<int> budget;
    int id;

    explicit Fragile(int i) : id(i) {}                 // not counted
    Fragile(const Fragile& o) : id(o.id) { spend(); }
    Fragile(Fragile&& o) : id(o.id) { spend(); }
    Fragile& operator=(const Fragile& o) { spend(); id = o.id; return *this; }
    Fragile& operator=(Fragile&& o) { spend(); id = o.id; return *this; }
    ~Fragile() = default;

    static void spend() {
        if (budget.fetch_sub(1) <= 0) throw std::runtime_error("T's copy constructor threw");
    }
};

std::atomic<int> Fragile::budget{1 << 29};

const int kOpen = 1 << 29;   // effectively "never throw"

const int kItems   = 3000;
const int kThreads = 8;
const int kTrials  = 8;

}  // namespace

int main() {
    // ── 1. basic behaviour of both overloads ─────────────────────────────
    {
        threadsafe_stack<int> s;
        if (s.pop() != nullptr) {
            std::printf("pop() on an empty stack should return nullptr\n");
            return 1;
        }
        int out = -1;
        if (s.pop(out)) {
            std::printf("pop(T&) on an empty stack should return false\n");
            return 1;
        }
        s.push(1);
        s.push(2);
        std::shared_ptr<int> a = s.pop();
        if (!a || *a != 2) {
            std::printf("expected 2 from the first pop, got %d\n", a ? *a : -1);
            return 1;
        }
        if (!s.pop(out) || out != 1) {
            std::printf("expected 1 from pop(T&), got %d\n", out);
            return 1;
        }
        if (!s.empty()) { std::printf("stack should be empty now\n"); return 1; }
    }

    // ── 2. a throwing T must never destroy an element ────────────────────
    // Push M elements. Allow exactly `budget` more copies/moves to succeed,
    // then attempt one pop. The pop is allowed to throw. What is not allowed
    // is for the element to vanish: after the dust settles, draining the stack
    // plus whatever the pop returned must account for all M ids.
    const int kM = 6;
    for (int use_ref = 0; use_ref < 2; ++use_ref) {
        for (int budget = 0; budget <= 3; ++budget) {
            Fragile::budget.store(kOpen);
            threadsafe_stack<Fragile> s;
            for (int i = 0; i < kM; ++i) s.push(Fragile(i));

            std::vector<int> recovered;
            std::string what;

            Fragile::budget.store(budget);
            try {
                if (use_ref) {
                    Fragile out(-1);
                    if (s.pop(out)) recovered.push_back(out.id);
                } else {
                    std::shared_ptr<Fragile> p = s.pop();
                    if (p) recovered.push_back(p->id);
                }
            } catch (const std::exception& e) {
                what = e.what();
            }
            Fragile::budget.store(kOpen);

            while (std::shared_ptr<Fragile> p = s.pop()) recovered.push_back(p->id);

            std::vector<int> seen(kM, 0);
            for (int id : recovered) {
                if (id < 0 || id >= kM) {
                    std::printf("%s / budget %d: recovered id %d, which was never pushed\n",
                                use_ref ? "pop(T&)" : "pop()", budget, id);
                    return 1;
                }
                ++seen[id];
            }
            for (int i = 0; i < kM; ++i) {
                if (seen[i] != 1) {
                    std::printf("%s with copy budget %d: id %d appeared %d times, expected once\n",
                                use_ref ? "pop(T&)" : "pop()", budget, i, seen[i]);
                    if (!what.empty())
                        std::printf("  the pop threw \"%s\"\n", what.c_str());
                    std::printf("  the element was removed from the stack and then lost while "
                                "being handed to the caller\n"
                                "  nothing in the program still holds it, so it cannot be "
                                "retried or recovered\n");
                    return 1;
                }
            }
        }
    }

    // ── 3. pop() is still atomic (part 0's property must survive) ────────
    for (int trial = 0; trial < kTrials; ++trial) {
        threadsafe_stack<int> s;
        for (int i = 0; i < kItems; ++i) s.push(i);

        std::vector<std::vector<int>> got(kThreads);
        std::vector<std::thread> ts;
        for (int t = 0; t < kThreads; ++t) {
            ts.emplace_back([&, t] {
                SHAKE();
                while (std::shared_ptr<int> p = s.pop()) got[t].push_back(*p);
                SHAKE();
            });
        }
        for (std::thread& th : ts) th.join();

        std::vector<int> seen(kItems, 0);
        long total = 0;
        for (const std::vector<int>& v : got) {
            for (int x : v) {
                ++total;
                if (x < 0 || x >= kItems) {
                    std::printf("trial %d: pop() returned %d, never pushed\n", trial, x);
                    return 1;
                }
                ++seen[x];
            }
        }
        int dup = 0, missing = 0;
        for (int i = 0; i < kItems; ++i) {
            if (seen[i] > 1) ++dup;
            if (seen[i] == 0) ++missing;
        }
        if (dup || missing || total != kItems) {
            std::printf("trial %d: %d duplicated, %d lost (%ld returned, %d pushed) — "
                        "pop() is not atomic\n", trial, dup, missing, total, kItems);
            return 1;
        }
    }

    std::printf("both overloads correct, no element lost to a throwing copy, "
                "%d threads x %d trials drained cleanly\n", kThreads, kTrials);
    return 0;
}
