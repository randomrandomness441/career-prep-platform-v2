The published solution waits for "the light is green for me" and *after* the
wait flips the light if it was wrong. Trace who can reach that flip: `cv.wait`
with a predicate only returns when the predicate is already true, so the
flipping code is unreachable dead code. Nobody ever changes the light. Ask:
at what moments *can* the light change, and who is holding the lock then?
There are exactly two: a car arriving at a red axis, and the last car leaving
an empty bridge.
---
You need more state than "which axis is green". Three pieces: `passing`
(cars on the bridge right now), `waiting[2]` (cars arrived but not yet
admitted, per axis), and a `closed` flag (the green axis has an opponent
queued, so no newcomers). The light may only switch while `passing == 0`, and
only if `waiting[other axis] > 0`. Do that check in both places from hint 1,
under the lock, and `notify_all` when it actually flips.
---
Admitting a car must not hold the lock through `cross_car()`, or same-axis
cars serialise and requirement 3 is gone. The pattern is: increment `passing`,
unlock, run the callback, relock, decrement. Make the decrement happen even if
the callback throws — a small RAII guard whose destructor relocks and does the
decrement plus the empty-bridge check. `closed` is what enforces requirement 4:
a green-axis arrival waits when `closed` is set, and `closed` is set by the
arrival of any car on the other axis.
