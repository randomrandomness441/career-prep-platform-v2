> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

You are given an `n x n` binary matrix `grid`. You can change at most one `0` to be a `1`. Return the size of the largest island in `grid` after applying this operation. An island is a 4-directionally connected group of `1`s.

```java
public int largestIsland(int[][] grid)
```

### Example 1:

```
Input: grid = [[1,0],[0,1]]
Output: 3
Explanation: Change one 0 to 1 and connect two 1s, then we get an island with area = 3.
```

### Example 2:

```
Input: grid = [[1,1],[1,0]]
Output: 4
Explanation: Change the 0 to 1 and make the island bigger, only one island possible.
```

### Example 3:

```
Input: grid = [[1,1],[1,1]]
Output: 4
Explanation: Can't change any 0 to 1, only one island possible.
```

### Constraints:

- `n == grid.length`
- `n == grid[i].length`
- `1 <= n <= 500`
- `grid[i][j]` is either 0 or 1.
