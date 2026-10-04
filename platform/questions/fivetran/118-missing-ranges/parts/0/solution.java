import java.util.*;

class Solution {
    public List<List<Integer>> findMissingRanges(int[] nums, int lower, int upper) {
        List<List<Integer>> result = new ArrayList<>();
        long prev = (long) lower - 1;
        for (int i = 0; i <= nums.length; i++) {
            long curr = (i == nums.length) ? (long) upper + 1 : nums[i];
            if (curr - prev > 1) {
                result.add(Arrays.asList((int) (prev + 1), (int) (curr - 1)));
            }
            prev = curr;
        }
        return result;
    }
}