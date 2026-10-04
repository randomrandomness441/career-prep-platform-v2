Compute all 6 pairwise squared distances between the 4 points (squared, so you stay in
integers and avoid `sqrt` entirely). Count how many distinct values appear and how many
times each appears.
---
A valid square has exactly 2 distinct distance values: the side length appears 4 times
(there are 4 sides among the 6 pairs), and the diagonal appears 2 times. Reject anything
else -- including a side length of 0 (coincident points).
---
```cpp
long side = *distances.begin();     // the smaller of the 2 distinct values
long diag = *std::next(distances.begin());
return side > 0 && diag == 2 * side
    && count(side) == 4 && count(diag) == 2;
```
Don't forget: reject `side == 0` explicitly, or four coincident points (all distances 0,
technically "1 distinct value repeated 6 times") slip through as a degenerate pass.
