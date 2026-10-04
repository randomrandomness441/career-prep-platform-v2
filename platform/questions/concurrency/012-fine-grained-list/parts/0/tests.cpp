// Harness for "A List You Can Walk While Someone Edits It".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <thread>
#include <vector>

namespace {

const int kTrials     = 8;
const int kPushers    = 4;
const int kPerPusher  = 250;                    // values 0 .. kPushers*kPerPusher-1
const int kTraversers = 2;

// The list can never hold more than kPushers*kPerPusher = 1000 elements, so a
// walk that gets this far is chasing a pointer into memory that was recycled
// behind it. Throwing is how we get out: the walk itself has no exit.
const size_t kWalkCap = 20000;

struct runaway {};

bool removable(int v) { return v % 3 == 0; }

void dump_mismatch(const char* what, const std::vector<int>& got,
                   const std::vector<int>& want) {
    std::printf("%s: %zu elements, expected %zu\n", what, got.size(), want.size());
    const size_t n = got.size() < want.size() ? got.size() : want.size();
    for (size_t i = 0; i < n; ++i) {
        if (got[i] != want[i]) {
            std::printf("first difference at position %zu: saw %d, expected %d\n",
                        i, got[i], want[i]);
            break;
        }
    }
}

// The list is corrupt at this point; unwinding through its destructor would walk
// the same broken pointers again. Say what happened and leave.
[[noreturn]] void bail() {
    std::fflush(stdout);
    std::exit(1);
}

}  // namespace

int main() {
    // ── 1. single-threaded behaviour ─────────────────────────────────────
    {
        threadsafe_list l;

        int count = 0;
        l.for_each([&](int) { ++count; });
        if (count != 0) { std::printf("a fresh list walked %d elements\n", count); return 1; }
        if (l.find_first_if([](int) { return true; })) {
            std::printf("find_first_if found something in an empty list\n");
            return 1;
        }
        l.remove_if([](int) { return true; });   // must survive an empty list

        for (int i = 0; i < 5; ++i) l.push_front(i);

        std::vector<int> got;
        l.for_each([&](int v) { got.push_back(v); });
        std::vector<int> want{4, 3, 2, 1, 0};
        if (got != want) { dump_mismatch("push_front must prepend", got, want); return 1; }

        const std::optional<int> hit = l.find_first_if([](int v) { return v == 2; });
        if (!hit || *hit != 2) { std::printf("find_first_if missed a value that is there\n"); return 1; }
        if (l.find_first_if([](int v) { return v == 99; })) {
            std::printf("find_first_if invented a value\n");
            return 1;
        }
        // List order is 4,3,2,1,0 — the first element under 3 is 2, not 0.
        const std::optional<int> first = l.find_first_if([](int v) { return v < 3; });
        if (!first || *first != 2) {
            std::printf("find_first_if must return the FIRST match in list order\n");
            return 1;
        }

        l.remove_if([](int v) { return v % 2 == 0; });   // drops 4, 2 and 0
        got.clear();
        l.for_each([&](int v) { got.push_back(v); });
        want = {3, 1};
        if (got != want) { dump_mismatch("remove_if (head, middle and tail)", got, want); return 1; }

        l.remove_if([](int v) { return v == 3; });       // the head element
        got.clear();
        l.for_each([&](int v) { got.push_back(v); });
        want = {1};
        if (got != want) { dump_mismatch("removing the first element", got, want); return 1; }

        l.remove_if([](int v) { return v == 1; });       // the last one
        got.clear();
        l.for_each([&](int v) { got.push_back(v); });
        if (!got.empty()) { std::printf("removing the last element left %zu behind\n", got.size()); return 1; }
        l.remove_if([](int) { return true; });           // empty again
    }

    // ── 2. what the whole question is about ──────────────────────────────
    // Pushers, one remover, two walkers and a searcher, all inside the list at
    // the same time.
    //
    // Three things are checked. A walk must terminate — the list holds at most
    // 1000 elements, so a walk that passes kWalkCap is following a pointer into
    // freed memory. A walk must never see the same value twice, since the values
    // are distinct and a walk only ever moves forward. And once every thread has
    // stopped, the surviving elements must be exactly the pushed values that the
    // removal predicate does not match: nothing lost, nothing resurrected.
    std::vector<int> want;
    for (int t = 0; t < kPushers; ++t)
        for (int i = 0; i < kPerPusher; ++i) {
            const int v = t * kPerPusher + i;
            if (!removable(v)) want.push_back(v);
        }
    std::sort(want.begin(), want.end());

    for (int trial = 0; trial < kTrials; ++trial) {
        threadsafe_list l;
        std::atomic<int>  pushers_left{kPushers};
        std::atomic<bool> corrupt{false};
        std::atomic<int>  repeat_value{-1};
        std::atomic<long> finds{0};

        std::vector<std::thread> ts;

        for (int t = 0; t < kPushers; ++t) {
            ts.emplace_back([&, t] {
                for (int i = 0; i < kPerPusher; ++i) {
                    l.push_front(t * kPerPusher + i);
                    SHAKE();
                }
                --pushers_left;
            });
        }

        ts.emplace_back([&] {
            while (pushers_left.load() > 0 && !corrupt.load()) {
                size_t steps = 0;
                try {
                    l.remove_if([&](int v) {
                        if (++steps > kWalkCap) throw runaway{};
                        return removable(v);
                    });
                } catch (const runaway&) {
                    corrupt.store(true);
                    break;
                }
                SHAKE();
            }
        });

        for (int t = 0; t < kTraversers; ++t) {
            ts.emplace_back([&] {
                std::vector<int> seen;
                while (pushers_left.load() > 0 && !corrupt.load()) {
                    seen.clear();
                    try {
                        l.for_each([&](int v) {
                            if (seen.size() >= kWalkCap) throw runaway{};
                            seen.push_back(v);
                        });
                    } catch (const runaway&) {
                        corrupt.store(true);
                        break;
                    }
                    std::sort(seen.begin(), seen.end());
                    const std::vector<int>::iterator it =
                        std::adjacent_find(seen.begin(), seen.end());
                    if (it != seen.end()) repeat_value.store(*it);
                    SHAKE();
                }
            });
        }

        ts.emplace_back([&] {
            while (pushers_left.load() > 0 && !corrupt.load()) {
                size_t steps = 0;
                try {
                    if (l.find_first_if([&](int v) {
                            if (++steps > kWalkCap) throw runaway{};
                            return v == 7;
                        }))
                        ++finds;
                } catch (const runaway&) {
                    corrupt.store(true);
                    break;
                }
                SHAKE();
            }
        });

        for (std::thread& th : ts) th.join();

        if (corrupt.load()) {
            std::printf("trial %d: a walk passed %zu steps through a list that can hold at "
                        "most %d elements.\n"
                        "It is following next pointers through nodes that were unlinked and "
                        "freed while it stood on them.\n",
                        trial, kWalkCap, kPushers * kPerPusher);
            bail();
        }
        if (repeat_value.load() != -1) {
            std::printf("trial %d: one walk of the list saw the value %d twice.\n"
                        "Values are unique and a walk only moves forward, so the walk "
                        "followed a next pointer into a node that had already been "
                        "unlinked and freed.\n", trial, repeat_value.load());
            bail();
        }

        // Sweep up whatever the concurrent remover did not reach.
        l.remove_if([](int v) { return removable(v); });
        std::vector<int> left;
        l.for_each([&](int v) { left.push_back(v); });
        std::sort(left.begin(), left.end());

        if (left != want) {
            std::printf("trial %d: the list does not contain what was pushed.\n", trial);
            dump_mismatch("surviving elements", left, want);
            std::printf("A concurrent unlink and a concurrent push_front both wrote the "
                        "same next pointer, and one of the two writes was lost.\n");
            return 1;
        }
    }

    std::printf("%d trials of %d pushers, 1 remover, %d walkers and a searcher sharing "
                "the list: no stale pointers followed, no elements lost\n",
                kTrials, kPushers, kTraversers);
    return 0;
}
