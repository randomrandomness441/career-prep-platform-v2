## 1. Reframe the problem

Counting every all-ones rectangle sounds like it needs checking every possible
rectangle directly, `O((rows·cols)²)` of them in the worst case. The trick that gets
this down to something reasonable: fix a row as the *bottom* of the rectangle, and
reduce the question to a 1D one, "for each column, how tall a solid run of ones ends
here?", then reuse that 1D histogram across every row as you sweep down the grid.

## 3. The broken version, first

The natural first draft builds that per-column histogram but forgets the reset:

```cpp
if (grid[r][c] == 1) height[c] += 1;
```

Run it on the standard example grid, `{{1,0,1},{1,1,0},{1,1,0}}`, expected answer 13:

```
classic example: got 19, expected 13
```

Column 0 is `1,1,1` top to bottom, no zeros, so the bug doesn't show up there. Column 2
is `1,0,0`, a single `1` followed by two `0`s. The correct height sequence for column 2
across the three rows is `1, 0, 0` (the `1` doesn't survive past the row after it).
Without the reset, it's `1, 1, 1`, the height froze at its last non-zero value instead
of falling back to zero, so every row after the `0` still "remembers" a tall column that
isn't actually there anymore, and the sweep counts rectangles that reach through a `0`
cell.

## 6. Where this solution fails

- **Rectangular (not just square) submatrices are already handled**, this is a common
 point of confusion given the reported title says "square submatrices," but the actual
 LeetCode problem (1504) this is modeled on counts *all* rectangles, not just squares.
 If the real ask is specifically squares, the running-minimum sum in the inner loop
 needs to only count the case where width equals height, which is a different (and
 smaller) sum than what's implemented here.
