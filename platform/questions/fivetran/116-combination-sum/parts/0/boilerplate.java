import java.util.*;

class Solution {
    public List<List<Integer>> combinationSum(int[] candidates, int target) {
        List<List<Integer>> result = new ArrayList<>();
        dfs(candidates, target, new ArrayList<>(), result);
        return result;
    }

    private void dfs(int[] candidates, int remaining, List<Integer> path, List<List<Integer>> result) {
        if (remaining == 0) {
            result.add(new ArrayList<>(path));
            return;
        }
        if (remaining < 0) {
            return;
        }
        for (int value : candidates) {
            path.add(value);
            dfs(candidates, remaining - value, path, result);
            path.remove(path.size() - 1);
        }
    }
}