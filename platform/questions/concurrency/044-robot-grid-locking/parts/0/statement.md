# Robot Grid: Fine-Grained Cell Locking

## ELI5: two people swapping seats in a movie theater at the same instant

Picture two people who decide, at the exact same moment, to swap seats with each other,
person A wants B's seat, and B wants A's, simultaneously. For the swap to happen cleanly,
each of them needs to briefly hold *both* seats at once (their old one and the new one).
If both of them grab their own current seat first and then reach for the other one, you
get the same standoff as [[005-scoped-lock]]'s bank transfer: A is holding seat A,
reaching for seat B; B is holding seat B, reaching for seat A. Neither lets go. Nobody
swaps, ever.

This question is that exact shape, but with robots and grid cells instead of people and
seats, and it comes up specifically because two robots each moving into the *other's*
current cell is a completely normal, common thing to happen on a busy grid, not some rare
edge case.

## What you're actually building

Many robots clean cells of a shared grid concurrently. A move between adjacent cells
needs exclusive access to both cells for the instant of the move.

```cpp
class RobotCleaner {
public:
    RobotCleaner(int rows, int cols);
    // Move a robot from (r1,c1) to the adjacent (r2,c2), cleaning the
    // target cell. Needs exclusive access to BOTH cells for the operation.
    void move(int r1, int c1, int r2, int c2);
};
```

## Requirements

1. `move` must have exclusive access to both `(r1,c1)` and `(r2,c2)` for the duration of
 the operation, no other robot may be mid-move into or out of either cell at the same
 time.
2. **Many robots call `move` concurrently, including cases where two robots are moving
 into each other's current cell at the same moment**, the seat-swap scenario above.
 This must never deadlock.
3. **One mutex per cell** (`rows * cols` of them), this is the fine-grained answer, not
 one mutex for the whole grid.

## Why the constraints exist

**Don't hold one cell's lock while blocking to acquire the other**, that ordering is
exactly what has to be avoided. Same lesson as [[005-scoped-lock]]: lock both resources
without ever standing there holding one while waiting on the other in a way that can
form a cycle.
