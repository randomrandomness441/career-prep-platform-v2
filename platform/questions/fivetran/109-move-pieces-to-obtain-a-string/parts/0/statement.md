Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

You are given two strings start and target, both of length n and containing only 'L', 'R', and '_'. A piece 'L' slides only left and a piece 'R' slides only right, each into an adjacent blank cell '_'. Return true if target can be reached from start after any number of moves, including zero moves.

public boolean canChange(String start, String target)

Example 1:
Input: start = "_L__R__R_", target = "L______RR"
Output: true

Example 2:
Input: start = "R_L_", target = "__LR"
Output: false

Example 3:
Input: start = "_R", target = "R_"
Output: false

Constraints:
- n == start.length == target.length
- 1 <= n <= 10^5
- start and target consist only of the characters 'L', 'R', and '_'.