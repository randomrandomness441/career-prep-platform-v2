# Ticket Reservation: Optimistic vs Pessimistic

## ELI5: a hundred people clicking "buy" on the same front-row seat, at once

A popular concert goes on sale, and the front-row seats get absolutely hammered, a
hundred people click "buy" on seat 5 within the same second. Exactly one of them can
actually walk away with that seat. The other ninety-nine need to hear "sorry, already
taken" *immediately*, not stand around wondering, and crucially, someone trying to buy
seat 12 at that same moment shouldn't be slowed down at all just because seat 5 is on
fire. Different seats are completely unrelated contests; only people fighting over the
*same* seat are actually racing each other.

## What you're actually building

Book seats for an event. Many users try to book the same handful of popular seats at
once, this is deliberately a high-contention design, not a low-contention one.

```cpp
class TicketBooking {
public:
    explicit TicketBooking(int num_seats);
    // Attempt to reserve `seat` for `user_id`. Returns true if THIS call
    // won the seat, false if it was already taken (by anyone).
    bool reserve(int seat, int user_id);
    int owner_of(int seat) const; // -1 if unreserved
};
```

## Requirements

1. For any seat, `reserve` must return `true` to **at most one** caller, across every
 thread, ever, no seat is ever double-booked.
2. A seat that's free must be reservable, `reserve` returning `false` for every caller
 on a genuinely free seat is as wrong as double-booking it.
3. `owner_of` must agree with whichever caller `reserve` actually told "you won."

## Why the constraints exist

- **No seat's reservation may block another seat's**, contention on seat 5 must not
 slow down a caller reserving seat 12. Unrelated seats are unrelated contests.
- **This is optimistic concurrency control**: no lock is held while "deciding" whether a
 reservation succeeds. The decision and the claim happen as one indivisible step,
 nobody stands in line waiting to even find out if the seat's available; you just try to
 grab it and immediately learn whether you got it.
