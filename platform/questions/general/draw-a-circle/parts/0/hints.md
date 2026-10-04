Compute just one octant — from angle 45° to 90°, where `x >= y` — using a running
"decision" value `d` that starts at `1 - r` and tells you, at each step, whether the
midpoint between two candidate next pixels is inside or outside the true circle:

```cpp
int x = r, y = 0, d = 1 - r;
while (x >= y) {
    // (x, y) is a point on this octant
    ++y;
    if (d <= 0) { d += 2*y + 1; }              // midpoint inside: move up only
    else        { --x; d += 2*(y - x) + 1; }   // midpoint outside: move up AND in
}
```
---
For every `(x, y)` this octant loop produces, mirror it into all 8 symmetric positions
around the center: `(±x, ±y)` and `(±y, ±x)`. That's the whole circle — one octant
computed, seven more obtained for free from symmetry, no trigonometry anywhere.
Collect into a `std::set<std::pair<int,int>>` rather than a plain vector, since the
octant boundary (`x == y`) naturally produces the same point from both mirrored
positions.
