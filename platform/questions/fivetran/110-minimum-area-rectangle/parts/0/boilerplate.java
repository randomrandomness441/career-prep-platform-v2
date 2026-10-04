import java.util.*;

class Solution {
    public int minAreaRect(int[][] points) {
        // Group points into columns by x, then look for two columns that share
        // the same pair of y values. Only adjacent y values in a sorted column
        // get paired up.
        Map<Integer, List<Integer>> columns = new TreeMap<>();
        for (int[] p : points) {
            columns.computeIfAbsent(p[0], k -> new ArrayList<>()).add(p[1]);
        }
        for (List<Integer> ys : columns.values()) {
            Collections.sort(ys);
        }

        Map<Long, Integer> lastColumn = new HashMap<>();
        int min = Integer.MAX_VALUE;
        for (Map.Entry<Integer, List<Integer>> entry : columns.entrySet()) {
            int x = entry.getKey();
            List<Integer> ys = entry.getValue();
            for (int i = 0; i + 1 < ys.size(); i++) {
                long key = (long) ys.get(i) * 40001L + ys.get(i + 1);
                Integer prevX = lastColumn.get(key);
                if (prevX != null) {
                    int area = (x - prevX) * (ys.get(i + 1) - ys.get(i));
                    if (area < min) {
                        min = area;
                    }
                }
                lastColumn.put(key, x);
            }
        }
        return min == Integer.MAX_VALUE ? 0 : min;
    }
}