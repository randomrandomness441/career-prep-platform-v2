import java.util.*;

class Solution {
    public int minAreaRect(int[][] points) {
        Set<Long> pointSet = new HashSet<>();
        for (int[] p : points) {
            pointSet.add(encode(p[0], p[1]));
        }

        int n = points.length;
        int min = Integer.MAX_VALUE;
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                int x1 = points[i][0], y1 = points[i][1];
                int x2 = points[j][0], y2 = points[j][1];
                if (x1 == x2 || y1 == y2) {
                    continue;
                }
                if (pointSet.contains(encode(x1, y2)) && pointSet.contains(encode(x2, y1))) {
                    int area = Math.abs(x1 - x2) * Math.abs(y1 - y2);
                    if (area < min) {
                        min = area;
                    }
                }
            }
        }
        return min == Integer.MAX_VALUE ? 0 : min;
    }

    private long encode(int x, int y) {
        return (long) x * 40001L + y;
    }
}