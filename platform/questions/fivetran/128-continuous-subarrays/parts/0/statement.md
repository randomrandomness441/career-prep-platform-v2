Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

You are given a 0-indexed integer array `nums`. A subarray of `nums` is continuous if for every pair of indices `i1 <= i2` inside the subarray, `|nums[i1] - nums[i2]| <= 2`. That is the same as requiring the subarray max minus the subarray min to be at most 2. Return the total number of non-empty continuous subarrays.

`public long continuousSubarrays(int[] nums)`

Input: nums = [5,4,2,4]
Output: 8
The continuous subarrays are [5], [4], [2], [4], [5,4], [4,2], [2,4], and [4,2,4].

Input: nums = [1,2,3]
Output: 6
Every subarray is continuous.

Input: nums = [1,3,5]
Output: 5
[1,3,5] itself does not count because |1 - 5| = 4.

Constraints:
- 1 <= nums.length <= 10^5
- 1 <= nums[i] <= 10^9