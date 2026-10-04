## The problem

Design a parking lot system that handles both 2-wheelers and 4-wheelers, for three kinds
of users: the lot **owner** (configures the lot, sets pricing), a **person parking**
(drives in, gets a slot, pays, drives out), and a **security guard** (checks people in and
out at the gate).

Cover, in your answer:

- The core entities and how they relate -- lot, floor, slot, vehicle, ticket, payment --
  and a DB schema for them (table names, key columns, relationships).
- What each of the three roles can actually do, as a set of operations/services.
- How a vehicle gets assigned a slot on entry, sized correctly for a 2-wheeler vs a
  4-wheeler, and what happens when the lot (or the right slot type) is full.
- How the ticket/payment flow works from entry to exit, and how the fee gets calculated.
- What happens when two vehicles arrive at the same gate at the same moment and there's
  exactly one slot of the right size left.

The interviewer will likely layer on a new requirement partway through (reserved slots,
multiple gates, an EV-charging slot type, a monthly pass) to see how the design holds up.
There's no code to write here -- answer in plain writing, the way you'd talk it through on
a whiteboard.
