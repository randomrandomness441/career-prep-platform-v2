class Solution {
    public long continuousSubarrays(int[] nums) {
        // A subarray is continuous as long as neighboring elements never
        // differ by more than 2, so track the most recent broken pair.
        long total = 0;
        int left = 0;
        int lastBad = -1;
        for (int right = 0; right < nums.length; right++) {
            if (right > 0 && Math.abs(nums[right] - nums[right - 1]) > 2) {
                lastBad = right - 1;
            }
            if (lastBad + 1 > left) {
                left = lastBad + 1;
            }
            total += right - left + 1;
        }
        return total;
    }
}