import java.util.*;

class Solution {
    public List<List<Integer>> findMissingRanges(int[] nums, int lower, int upper) {
        List<List<Integer>> result = new ArrayList<>();
        long next = lower;
        for (int i = 0; i < nums.length; i++) {
            if (nums[i] > next) {
                result.add(Arrays.asList((int) next, nums[i] - 1));
            }
            next = (long) nums[i] + 1;
        }
        return result;
    }
}