> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

You are given a 0-indexed integer array `nums` of length `n`. You start at `nums[0]`, and each `nums[i]` is the maximum length of a forward jump from index `i`. Return the minimum number of jumps needed to reach `nums[n - 1]`. The test cases are generated so that you can always reach the last index.

```java
public int jump(int[] nums)
```

### Example 1:

```
Input: nums = [2,3,1,1,4]
Output: 2
Explanation: Jump 1 step from index 0 to 1, then 3 steps to the last index.
```

### Example 2:

```
Input: nums = [2,3,0,1,4]
Output: 2
```

### Example 3:

```
Input: nums = [1]
Output: 0
```

### Constraints:

- `1 <= nums.length <= 10^4`
- `0 <= nums[i] <= 1000`
- It is guaranteed that you can reach `nums[n - 1]`.
