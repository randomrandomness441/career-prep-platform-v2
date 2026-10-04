#include <atomic>
#include <vector>

// Optimistic concurrency control: no lock is ever held. A reservation
// attempt is a single compare-and-swap on that seat's owner -- "if this
// seat is still unowned, make it mine" -- and either it wins outright or it
// finds out immediately that someone else already won, with nothing in
// between where two callers could both believe they succeeded.
class TicketBooking {
public:
    explicit TicketBooking(int num_seats) : owner_(static_cast<std::size_t>(num_seats)) {
        for (auto& o : owner_) o.store(-1, std::memory_order_relaxed);
    }

    // Returns true if THIS call won the seat, false if it was already taken
    // (by anyone -- including a concurrent caller that raced this one).
    bool reserve(int seat, int user_id) {
        int expected = -1;
        return owner_[static_cast<std::size_t>(seat)]
            .compare_exchange_strong(expected, user_id, std::memory_order_acq_rel,
                                      std::memory_order_relaxed);
    }

    int owner_of(int seat) const {
        return owner_[static_cast<std::size_t>(seat)].load(std::memory_order_acquire);
    }

private:
    std::vector<std::atomic<int>> owner_;
};
