> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

You are given an inclusive range `[lower, upper]` and a sorted array `nums` of unique integers. Every element of `nums` lies inside that range. A number `x` is considered missing if `x` is in `[lower, upper]` and `x` is not in `nums`. Return the shortest sorted list of ranges that exactly covers the missing numbers. No element of `nums` appears in any returned range, and every missing number is covered by exactly one range.

```java
public List<List<Integer>> findMissingRanges(int[] nums, int lower, int upper)
```

### Example 1:

```
Input: nums = [0,1,3,50,75], lower = 0, upper = 99
Output: [[2,2],[4,49],[51,74],[76,99]]
```

### Example 2:

```
Input: nums = [-1], lower = -2, upper = -1
Output: [[-2,-2]]
```

### Example 3:

```
Input: nums = [], lower = 1, upper = 1
Output: [[1,1]]
```

### Constraints:

- `-10^9 <= lower <= upper <= 10^9`
- `0 <= nums.length <= 100`
- `lower <= nums[i] <= upper`
- All values in `nums` are unique, and `nums` is sorted in ascending order.
