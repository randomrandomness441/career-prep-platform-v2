# Determine Whether Four Points Form a Square

## ELI5: four dots on paper, in no particular order

Someone hands you four dots on a piece of graph paper and asks "do these form a
square?" They didn't label which dot is "top-left" or give them to you walking around
the shape in order, they're just four `(x, y)` pairs, in whatever order they happened
to be listed. Your job is to figure out, from the raw coordinates alone, whether there's
*some* way to connect these four dots into a square, not whether they're already
listed in square-walking order.

## What you're actually building

```cpp
struct Point { int x, y; };
bool is_valid_square(Point p1, Point p2, Point p3, Point p4);
```

Return `true` if the four points, taken as a set (their order in the argument list
doesn't matter), form a square with positive area, `false` otherwise, including when
they're degenerate (coincide, are collinear, or form some other quadrilateral).

## Requirements

1. Works regardless of which order the four points are passed in, adjacent corners,
 diagonal corners, whatever.
2. Four coincident points (all the same point) must return `false`, a square needs
 positive area.
3. Handles axis-aligned squares, rotated squares, and squares of any size.

## Why the constraints exist

**A square is fully determined by its six pairwise distances, not by walking its
edges in order.** Four corners of a square produce exactly six pairwise distances: four
equal "sides" and two equal, longer "diagonals," with `diagonal² == 2 × side²`
(Pythagoras, since a square's diagonal cuts it into two right triangles). That
distance-based check works no matter what order the points are given in, which is
exactly the property a naive "check the four edges in argument order" approach doesn't
have.
