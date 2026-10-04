import java.util.*;

class Solution {
    public long continuousSubarrays(int[] nums) {
        Deque<Integer> maxDeque = new ArrayDeque<>(); // indices, values decreasing
        Deque<Integer> minDeque = new ArrayDeque<>(); // indices, values increasing
        long total = 0;
        int left = 0;
        for (int right = 0; right < nums.length; right++) {
            while (!maxDeque.isEmpty() && nums[maxDeque.peekLast()] <= nums[right]) {
                maxDeque.pollLast();
            }
            maxDeque.addLast(right);

            while (!minDeque.isEmpty() && nums[minDeque.peekLast()] >= nums[right]) {
                minDeque.pollLast();
            }
            minDeque.addLast(right);

            while ((long) nums[maxDeque.peekFirst()] - nums[minDeque.peekFirst()] > 2L) {
                if (maxDeque.peekFirst() == left) {
                    maxDeque.pollFirst();
                }
                if (minDeque.peekFirst() == left) {
                    minDeque.pollFirst();
                }
                left++;
            }

            total += right - left + 1;
        }
        return total;
    }
}