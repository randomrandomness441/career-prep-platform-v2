#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    constexpr int kThreads = 8;
    constexpr long kTickets = 100000;

    // ── Phase 1: all threads at once. Each thread issues kTickets tickets and
    // must see exactly 1, 2, 3, ... — its own sequence, undisturbed by anyone.
    std::atomic<long> grand_total{0};
    std::vector<int> station(kThreads, -1);
    std::vector<int> rc(kThreads, 0);
    std::vector<long> first_bad(kThreads, 0);
    std::vector<long> first_expected(kThreads, 0);
    {
        std::vector<std::thread> threads;
        threads.reserve(kThreads);
        for (int i = 0; i < kThreads; ++i) {
            threads.emplace_back([i, &station, &rc, &first_bad, &first_expected,
                                  &grand_total] {
                station[i] = ticket_station::station_id();
                long expect = 1;
                for (long k = 0; k < kTickets; ++k) {
                    long got = ticket_station::next_ticket();
                    if (got != expect && rc[i] == 0) {
                        rc[i] = 1;
                        first_bad[i] = got;
                        first_expected[i] = expect;
                    }
                    ++expect;
                    if ((k & 1023) == 1023) SHAKE();
                }
                if (ticket_station::issued_here() != kTickets && rc[i] == 0)
                    rc[i] = 2;
                grand_total.fetch_add(ticket_station::issued_here(),
                                      std::memory_order_relaxed);
            });
        }
        for (auto& t : threads) t.join();
    }

    for (int i = 0; i < kThreads; ++i) {
        if (rc[i] == 1) {
            std::printf("thread %d (station %d): ticket %lld, expected %lld — "
                        "this counter is shared with another thread\n",
                        i, station[i], (long long)first_bad[i],
                        (long long)first_expected[i]);
            return 1;
        }
        if (rc[i] == 2) {
            std::printf("thread %d: issued_here() = %lld, expected %lld\n",
                        i, (long long)ticket_station::issued_here(), (long long)kTickets);
            return 1;
        }
    }

    // Station ids: all distinct, all from the dispenser's range.
    {
        bool seen[256] = {};
        for (int i = 0; i < kThreads; ++i) {
            if (station[i] < 0 || station[i] >= 256 || seen[station[i]]) {
                std::printf("station ids not unique: thread %d has %d\n", i, station[i]);
                return 1;
            }
            seen[station[i]] = true;
        }
    }

    if (grand_total.load() != (long)kThreads * kTickets) {
        std::printf("grand total %lld, expected %lld\n",
                    (long long)grand_total.load(), (long long)kThreads * kTickets);
        return 1;
    }

    // ── Phase 2: a NEW thread starts from scratch — its own counter at 1, its
    // own station id from the dispenser. State belongs to the thread, not the
    // process.
    {
        std::atomic<long> first{0};
        std::atomic<long> second{0};
        std::atomic<int> sid{-1};
        std::thread t([&] {
            first = ticket_station::next_ticket();
            second = ticket_station::next_ticket();
            sid = ticket_station::station_id();
        });
        t.join();
        if (first.load() != 1 || second.load() != 2) {
            std::printf("new thread's first tickets: %lld, %lld — expected 1, 2\n",
                        (long long)first.load(), (long long)second.load());
            return 1;
        }
        if (sid.load() != kThreads) {
            std::printf("new thread's station id: %d, expected %d\n",
                        sid.load(), kThreads);
            return 1;
        }
    }

    std::printf("ticket_station: %d threads x %ld tickets, sequences intact, "
                "ids unique, new threads start clean\n",
                kThreads, kTickets);
    return 0;
}
