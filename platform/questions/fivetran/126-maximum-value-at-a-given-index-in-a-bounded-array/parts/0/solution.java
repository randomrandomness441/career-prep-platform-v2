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
    // directions, but never below 1 since every element is a positive integer.
    private long minSum(long peak, long leftLen, long rightLen) {
        return peak + sideSum(peak, leftLen) + sideSum(peak, rightLen);
    }

    private long sideSum(long peak, long len) {
        if (len < peak) {
            // Values peak-1, peak-2, ..., peak-len, and all of them are at least 1.
            long smallest = peak - len;
            return (peak - 1 + smallest) * len / 2;
        }
        // Values peak-1, ..., 1, and then every remaining cell on this side is 1.
        long full = peak - 1;
        return full * (full + 1) / 2 + (len - full);
    }
}