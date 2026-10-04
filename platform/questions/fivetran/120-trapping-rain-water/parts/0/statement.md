> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

You are given an array of non-negative integers `height` where each element is the height of a bar of width 1. After it rains, water collects in the gaps between bars. Return the total units of water the elevation map traps.

```java
public int trap(int[] height)
```

### Example 1:

```
Input: height = [0,1,0,2,1,0,1,3,2,1,2,1]
Output: 6
```

### Example 2:

```
Input: height = [4,2,0,3,2,5]
Output: 9
```

### Example 3:

```
Input: height = [3,3,3]
Output: 0
```

### Constraints:

- `n == height.length`
- `1 <= n <= 2 * 10^4`
- `0 <= height[i] <= 10^5`
