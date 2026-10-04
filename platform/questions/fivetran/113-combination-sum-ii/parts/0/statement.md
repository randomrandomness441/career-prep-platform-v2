Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

Given a collection of candidate numbers `candidates` and a target number `target`, find all unique combinations in `candidates` where the numbers sum to `target`. Each number in `candidates` may be used at most once in each combination. The solution set must not contain duplicate combinations.

`public List<List<Integer>> combinationSum2(int[] candidates, int target)`

Input: candidates = [10,1,2,7,6,1,5], target = 8
Output: [[1,1,6],[1,2,5],[1,7],[2,6]]

Input: candidates = [2,5,2,1,2], target = 5
Output: [[1,2,2],[5]]

Constraints:
- 1 <= candidates.length <= 100
- 1 <= candidates[i] <= 50
- 1 <= target <= 30