## The problem

Design the booking core of a system like booking.com: a user browses inventory (hotel
rooms, or any bookable unit), holds one while they pay, and the hold either turns into a
confirmed booking or expires back into available inventory.

Cover, in your answer:

- The core entities -- User, Inventory (the bookable unit), Booking, Payment -- and how
  they relate.
- The booking lifecycle as an explicit state machine: what states a booking moves through
  from the moment a user picks a unit to the moment it's confirmed or cancelled, and what
  triggers each transition.
- What happens to a hold that's never paid for -- how it expires and the inventory becomes
  available again, without a background process needing to sweep constantly.
- Two users trying to book the very last unit of inventory at the same moment: what
  actually prevents both of them from getting a confirmed booking.
- How you'd extend this to add seat selection (the user picks a specific seat/room number,
  not just "one of N identical units") without redesigning the whole booking flow.

There's no code to write here. Answer in plain writing, the way you'd talk it through on a
whiteboard.
