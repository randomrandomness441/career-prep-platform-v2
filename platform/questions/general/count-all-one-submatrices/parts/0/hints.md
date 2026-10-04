Build a `height[]` array, one entry per column, updated as you sweep row by row:
`height[c] = grid[r][c] == 1 ? height[c] + 1 : 0` — the *reset to 0* on a `0` cell is
what makes this a column of *consecutive* ones ending here, not a running total.
---
For each row (after updating `height[]`), and each column `c` as a right edge, walk
left tracking the running minimum height:

```cpp
for (int c = 0; c < cols; ++c) {
    int min_h = height[c];
    for (int left = c; left >= 0 && height[left] > 0; --left) {
        min_h = std::min(min_h, height[left]);
        total += min_h;
    }
}
```
Every step of that inner loop adds the count of rectangles whose bottom-right corner is
`(r, c)` and whose left edge is `left` — the running minimum is exactly "how tall can a
rectangle spanning columns `left..c` be, given the shortest column in that span."
