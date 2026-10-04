## 1. Reframe the problem

A circle drawn on a screen has to answer a question trigonometry doesn't naturally
answer: which *whole pixels* does the curve pass through? `sin`/`cos` give you exact
real-number positions; a screen only has integer ones. The midpoint circle algorithm
sidesteps the rounding question entirely by staying in integers from the start,
deciding, with a single comparison per step, whether the next pixel over or the next
pixel diagonally is the better approximation to the true curve.

## 3. The broken version, first

The natural first draft steps an angle around the circle and rounds each position to
the nearest pixel:

```cpp
for (int deg = 0; deg < 360; deg += 5) {
    double theta = deg * PI / 180.0;
    int x = cx + std::lround(r * std::cos(theta));
    int y = cy + std::lround(r * std::sin(theta));
    pts.insert({x, y});
}
```

At a small radius this looks fine, nearby angles round to the same or adjacent
pixels either way, so a 5-degree step doesn't visibly miss anything. Run it at a larger
radius, where it actually matters:

```
naive circle_points(0,0,20): 72 points
(the true outline at r=20 has 112 points)
```

40 pixels short, more than a third of the true outline, silently missing. The true
circle has *more* distinct boundary pixels to visit as `r` grows (the outline's actual
pixel-length scales with `r`), but this loop always visits the same 72 fixed angles
regardless of `r`, so the angular gap between consecutive samples corresponds to a
growing *pixel* gap as the radius increases, and past some radius, pixels the true
circle passes through are never sampled at all.

## 6. Where this solution fails

- **Ellipses, or any non-circular curve.** The midpoint algorithm's symmetry trick (one
 octant, mirrored eight ways) relies specifically on a circle's 8-fold symmetry. An
 ellipse only has 4-fold symmetry (or less, if rotated), so this exact approach
 doesn't generalize, a real "midpoint ellipse algorithm" exists but needs its own
 two-region derivation, not just a stretched version of this one.
- **Very large radii and integer overflow.** `d` and the running sums inside the loop
 grow roughly with `r`; for a radius large enough to threaten `int` overflow, the
 same "narrow to a smaller type than the computation needs" lesson from
 [[collatz-conjecture]] applies here too.
