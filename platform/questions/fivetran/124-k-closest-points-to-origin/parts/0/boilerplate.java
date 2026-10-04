import java.util.*;

class Solution {
    public int[][] kClosest(int[][] points, int k) {
        // Cap the heap at k so it holds the final answer.
        PriorityQueue<int[]> heap = new PriorityQueue<>(
                (a, b) -> Integer.compare(a[0] * a[0] + a[1] * a[1],
                                          b[0] * b[0] + b[1] * b[1]));
        for (int[] p : points) {
            heap.offer(p);
            if (heap.size() > k) {
                heap.poll();
            }
        }
        int[][] res = new int[k][];
        for (int i = 0; i < k; i++) {
            res[i] = heap.poll();
        }
        return res;
    }
}