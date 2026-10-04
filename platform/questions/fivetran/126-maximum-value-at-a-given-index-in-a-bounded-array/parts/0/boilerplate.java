import java.util.*;

class Solution {
    public int maxValue(int n, int index, int maxSum) {
        long lo = 1;
        long hi = maxSum;
        long leftLen = index;
        long rightLen = n - index - 1;
        long best = 1;
        while (lo <= hi) {
            long mid = lo + (hi - lo) / 2;
            if (minSum(mid, leftLen, rightLen) <= maxSum) {
                best = mid;
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }
        return (int) best;
    }

    // Smallest possible total when nums[index] == peak: ramp down by 1 in both
    // directions and let every other cell be as small as the ramp allows.
    private long minSum(long peak, long leftLen, long rightLen) {
        return peak + sideSum(peak, leftLen) + sideSum(peak, rightLen);
    }

    private long sideSum(long peak, long len) {
        if (len == 0) return 0;
        long smallest = peak - len;
        return (peak - 1 + smallest) * len / 2;
    }
}