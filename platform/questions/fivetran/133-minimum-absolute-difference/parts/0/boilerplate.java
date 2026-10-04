import java.util.*;

class Solution {
    public List<List<Integer>> minimumAbsDifference(int[] arr) {
        int[] sorted = arr.clone();
        Arrays.sort(sorted);
        int minDiff = Integer.MAX_VALUE;
        List<List<Integer>> result = new ArrayList<>();
        for (int i = 1; i < sorted.length; i++) {
            int diff = sorted[i] - sorted[i - 1];
            if (diff < minDiff) {
                minDiff = diff;
            }
            if (diff == minDiff) {
                result.add(Arrays.asList(sorted[i - 1], sorted[i]));
            }
        }
        return result;
    }
}