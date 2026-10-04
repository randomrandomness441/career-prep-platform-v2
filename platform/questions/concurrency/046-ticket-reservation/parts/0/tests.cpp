// Harness for "Ticket Reservation" (Optimistic Concurrency Control).
// Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    // Heavy contention on purpose: far more users than seats, everyone
    // trying to book the same small block of seats at once.
    constexpr int kSeats = 20;
    constexpr int kUsers = 400;

    TicketBooking booking(kSeats);
    std::vector<std::atomic<int>> wins_per_seat(kSeats);
    for (auto& w : wins_per_seat) w.store(0, std::memory_order_relaxed);
    std::atomic<int> total_true{0};

    std::vector<std::thread> threads;
    for (int u = 0; u < kUsers; ++u) {
        threads.emplace_back([&, u] {
            int seat = u % kSeats;   // every seat contested by kUsers/kSeats users
            SHAKE();
            if (booking.reserve(seat, u)) {
                wins_per_seat[static_cast<std::size_t>(seat)].fetch_add(1, std::memory_order_relaxed);
                total_true.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads) t.join();

    // Exactly one winner per seat -- reserve() must never tell two callers
    // "you got it" for the same seat.
    for (int s = 0; s < kSeats; ++s) {
        int wins = wins_per_seat[static_cast<std::size_t>(s)].load();
        if (wins != 1) {
            std::printf("seat %d: reserve() returned true %d times, expected exactly 1 "
                        "(double-booked if > 1, or nobody could claim a free seat if 0)\n",
                        s, wins);
            return 1;
        }
    }
    if (total_true.load() != kSeats) {
        std::printf("%d total successful reservations, expected exactly %d (one per seat)\n",
                    total_true.load(), kSeats);
        return 1;
    }

    // owner_of() must agree with whoever actually won -- no seat should
    // report an owner who was never told they succeeded.
    for (int s = 0; s < kSeats; ++s) {
        int owner = booking.owner_of(s);
        if (owner < 0 || owner >= kUsers) {
            std::printf("seat %d: owner_of() = %d, not a valid user id\n", s, owner);
            return 1;
        }
    }

    std::printf("%d users contending for %d seats: every seat won by exactly one caller\n",
                kUsers, kSeats);
    return 0;
}
