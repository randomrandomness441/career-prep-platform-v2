> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

The n-queens puzzle is the problem of placing `n` queens on an `n x n` chessboard so that no two queens attack each other. A queen attacks along its row, its column, and both diagonals. Given an integer `n`, return all distinct solutions as boards of `n` strings using `'Q'` for a queen and `'.'` for an empty square, in any order.

```java
public List<List<String>> solveNQueens(int n)
```

### Example 1:

```
Input: n = 4
Output: [[".Q..","...Q","Q...","..Q."],["..Q.","Q...","...Q",".Q.."]]
Explanation: Exactly two distinct solutions exist.
```

### Example 2:

```
Input: n = 1
Output: [["Q"]]
```

### Example 3:

```
Input: n = 2
Output: []
```

### Constraints:

- `1 <= n <= 9`
