Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

You are given an array of points in the X-Y plane `points`, where `points[i] = [xi, yi]`. Return the minimum area of a rectangle formed from these points, with sides parallel to the X and Y axes. If no such rectangle exists, return `0`.

`public int minAreaRect(int[][] points)`

Example 1:
Input: points = [[1,1],[1,3],[3,1],[3,3],[2,2]]
Output: 4
Explanation: The rectangle has corners (1,1), (1,3), (3,1), and (3,3), so its area is 2 * 2 = 4. The point (2,2) lies inside it and cannot serve as a corner.

Example 2:
Input: points = [[1,1],[1,3],[3,1],[3,3],[4,1],[4,3]]
Output: 2
Explanation: The rectangle has corners (3,1), (3,3), (4,1), and (4,3), so its area is 1 * 2 = 2.

Example 3:
Input: points = [[1,1],[2,2],[3,3]]
Output: 0
Explanation: No rectangle can be formed from collinear points.

Constraints:
- 1 <= points.length <= 500
- points[i].length == 2
- 0 <= points[i][0], points[i][1] <= 4 * 10^4
- All the given points are unique