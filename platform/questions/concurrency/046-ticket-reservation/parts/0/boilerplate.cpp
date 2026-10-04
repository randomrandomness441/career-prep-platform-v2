#include <vector>

// A fixed set of seats. reserve(seat, user_id) claims a seat for that user
// if nobody has claimed it yet. Two callers can race for the same seat at
// the same moment: at most one may ever be told it succeeded.
class TicketBooking {
public:
    explicit TicketBooking(int num_seats) : owner_(static_cast<std::size_t>(num_seats), -1) {}

    // Returns true if THIS call won the seat, false if it was already
    // taken (by anyone, including a concurrent caller that raced this one).
    bool reserve(int seat, int user_id) {
        // TODO: implement
        (void)seat;
        (void)user_id;
        return false;
    }

    int owner_of(int seat) const { return owner_[static_cast<std::size_t>(seat)]; }

private:
    std::vector<int> owner_;
};
