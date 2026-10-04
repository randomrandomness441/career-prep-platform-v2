## 1. Reframe the problem

"Do these 4 points form a square" sounds like a shape-walking problem, check each edge,
check each angle, but the moment you notice the points can arrive in *any* order, edge-
walking stops working, because you don't actually know which pairs are edges and which
are diagonals. The property that survives any ordering is the *set* of pairwise
distances: a square has exactly two distinct distance values among its six pairs (four
equal sides, two equal diagonals), and that's true no matter which order you're handed
the four corners in.

## 3. The broken version, first

The natural first draft assumes `p1, p2, p3, p4` are given walking around the square:

```cpp
long e1=dist2(p1,p2), e2=dist2(p2,p3), e3=dist2(p3,p4), e4=dist2(p4,p1);
long d1=dist2(p1,p3), d2=dist2(p2,p4);
return e1>0 && e1==e2 && e2==e3 && e3==e4 && d1==d2 && d1==2*e1;
```

Run it on the same four corners of a unit square, in two different orders:

```
4 points, walk order (p1,p2,p3,p4 adjacent): true
SAME 4 points, diagonal order (p1,p3 opposite): false
```

Identical square, identical four points, the only thing that changed is which one got
called `p1` versus `p3`. The moment `p1` and `p3` are opposite corners instead of
adjacent ones, `dist2(p1,p2)` is measuring a diagonal while the code is treating it as
an edge, and the whole check falls apart. Nothing in the problem statement promises an
order, so this "works on my test case" version fails silently on the exact same input
relabeled.

## 6. Where this solution fails

- **Floating-point coordinates.** With `int` coordinates the squared-distance approach
 is exact; switch to `double` and `diag == 2 * side` needs an epsilon comparison instead
 of `==`, or accumulated rounding error will reject genuine squares.
- **Very large coordinates.** Squaring a coordinate near `INT_MAX` overflows a 32-bit
 intermediate; the solution here promotes to `long` specifically to give headroom, but a
 `long` isn't infinite either, real production code would want an explicit bound check
 or a wider integer type if inputs are untrusted.
