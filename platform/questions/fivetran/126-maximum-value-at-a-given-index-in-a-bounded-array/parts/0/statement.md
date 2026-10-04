> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

You are given three integers `n`, `index`, and `maxSum`. You want to construct an array `nums` (0-indexed) of length `n` where every element is a positive integer, `abs(nums[i] - nums[i+1]) <= 1` for all valid `i`, and the sum of all elements does not exceed `maxSum`. Return the maximum possible value of `nums[index]` over all arrays that meet these conditions.

```java
public int maxValue(int n, int index, int maxSum)
```

### Example 1:

```
Input: n = 4, index = 2, maxSum = 6
Output: 2
Explanation: nums = [1,2,2,1] has sum 6 and nums[2] = 2.
```

### Example 2:

```
Input: n = 6, index = 1, maxSum = 10
Output: 3
Explanation: nums = [2,3,2,1,1,1] has sum 10 and nums[1] = 3.
```

### Example 3:

```
Input: n = 4, index = 0, maxSum = 4
Output: 1
Explanation: The only valid array is [1,1,1,1], so nums[0] = 1.
```

### Constraints:

- `1 <= n <= maxSum <= 10^9`
- `0 <= index < n`
