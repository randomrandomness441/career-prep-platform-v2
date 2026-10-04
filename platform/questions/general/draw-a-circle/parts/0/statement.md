# Draw a Circle (Midpoint Algorithm)

## ELI5: coloring in a circle on graph paper, one whole square at a time

A screen is graph paper, every pixel is a whole square, no fractional squares
allowed. "Draw a circle" really means: which *whole* squares does a mathematically
perfect circle pass closest to? Get this wrong and the circle looks like a lumpy,
gap-toothed mess instead of a smooth curve, even though the math behind it is exact.

## What you're actually building

```cpp
std::vector<std::pair<int,int>> circle_points(int cx, int cy, int r);
```

Return every unique integer pixel `(x, y)` that lies on the outline of a circle of
radius `r` centered at `(cx, cy)`, no gaps, no duplicates, order doesn't matter.

## Requirements

1. `r == 0` returns just the center point.
2. Every pixel returned is a genuine point on the circle's outline (not inside it, not
 outside it).
3. The outline has no gaps, every pixel a continuous circle would visually pass
 through at this radius is present.
4. No duplicate coordinates in the output.

## Why the constraints exist

**Stay in integer arithmetic the entire time, no `sin`/`cos`, no floating point.** The
classic approach (the midpoint circle algorithm) computes one octant of the circle
using only integer comparisons and additions, then mirrors it eight ways using the
circle's symmetry (swap `x`/`y`, negate either one) to get the other seven. A
trigonometric approach, stepping an angle from 0 to 360 degrees and rounding
`(r·cos θ, r·sin θ)` to the nearest pixel, looks reasonable and reliably produces
gaps: no fixed angle step lands exactly on every boundary pixel for every radius, and
the gaps get worse specifically at large radii, where consecutive pixels on the true
circle correspond to a smaller and smaller angular step than whatever fixed step was
chosen.
