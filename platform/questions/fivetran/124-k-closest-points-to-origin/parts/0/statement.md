> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

Given an array `points` where `points[i] = [xi, yi]` is a point on the X-Y plane and an integer `k`, return the `k` points closest to the origin `(0, 0)`. Closeness is Euclidean distance, so comparing squared distances is enough and you never need a square root. The answer is guaranteed unique except for order, so any ordering of the `k` points is accepted.

```java
public int[][] kClosest(int[][] points, int k)
```

### Example 1:

```
Input: points = [[1,3],[-2,2]], k = 1
Output: [[-2,2]]
```

### Example 2:

```
Input: points = [[3,3],[5,-1],[-2,4]], k = 2
Output: [[3,3],[-2,4]]
```

### Example 3:

```
Input: points = [[0,1],[2,0],[1,1]], k = 1
Output: [[0,1]]
```

### Constraints:

- `1 <= k <= points.length <= 10^4`
- `-10^4 <= xi, yi <= 10^4`
