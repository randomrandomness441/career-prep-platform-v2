# Count Submatrices With All Ones

## ELI5: how many solid rectangles are hiding in a grid of light switches

Picture a grid of light switches, some on (1) some off (0). A "solid rectangle" is any
contiguous block of switches that are *all* on, could be 1x1 (a single switch), could
be the whole grid if every switch happens to be on. Your job is to count every single
one of these solid rectangles hiding in the grid, at every size and every position.

## What you're actually building

```cpp
long long count_submatrices(const std::vector<std::vector<int>>& grid);
```

`grid[r][c]` is `0` or `1`. Count every contiguous rectangular submatrix (any width,
any height, any position) whose cells are entirely `1`.

## Requirements

1. A single `1` cell counts as its own 1x1 submatrix.
2. Every rectangle, of every size, made entirely of `1`s, gets counted, not just the
 largest ones.
3. Works on a grid that's all `0`s (answer: 0) and a grid that's all `1`s (answer: the
 count of every possible rectangle in it).

## Why the constraints exist

**Treat each row as the *bottom* of some rectangle, and track how tall a solid column
of 1s is at each position going into it.** For row `r`, column `c`, `height[c]` is how
many consecutive `1`s stack up ending at `(r, c)`, reset to 0 the instant a `0`
appears. Walking left from any column `c`, the running minimum of `height` tells you,
for every possible left edge, how tall a rectangle ending at column `c` and row `r` can
be, and summing that running minimum as you walk counts every rectangle with `(r, c)`
as its bottom-right corner, all at once.
