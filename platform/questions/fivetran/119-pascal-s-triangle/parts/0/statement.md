Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

Given an integer `numRows`, return the first `numRows` rows of Pascal's triangle. In Pascal's triangle, each number is the sum of the two numbers directly above it. The triangle starts with a single 1 at the top.

`public List<List<Integer>> generate(int numRows)`

Input: numRows = 5
Output: [[1],[1,1],[1,2,1],[1,3,3,1],[1,4,6,4,1]]

Input: numRows = 1
Output: [[1]]

Constraints:
- 1 <= numRows <= 30