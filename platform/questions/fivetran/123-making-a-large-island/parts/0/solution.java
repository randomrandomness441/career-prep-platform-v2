import java.util.*;

class Solution {

    private static final int[] DR = {-1, 1, 0, 0};
    private static final int[] DC = {0, 0, -1, 1};

    public int largestIsland(int[][] grid) {
        int n = grid.length;
        Map<Integer, Integer> sizes = new HashMap<>();
        int nextColor = 2;
        int best = 0;

        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                if (grid[r][c] == 1) {
                    int size = paint(grid, r, c, nextColor);
                    sizes.put(nextColor, size);
                    nextColor++;
                    best = Math.max(best, size);
                }
            }
        }

        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                if (grid[r][c] == 0) {
                    Set<Integer> seen = new HashSet<>();
                    int joined = 1;
                    for (int d = 0; d < 4; d++) {
                        int nr = r + DR[d];
                        int nc = c + DC[d];
                        if (nr >= 0 && nr < n && nc >= 0 && nc < n && grid[nr][nc] > 1 && seen.add(grid[nr][nc])) {
                            joined += sizes.get(grid[nr][nc]);
                        }
                    }
                    best = Math.max(best, joined);
                }
            }
        }

        return best;
    }

    private int paint(int[][] grid, int r, int c, int color) {
        int n = grid.length;
        int count = 0;
        Deque<int[]> stack = new ArrayDeque<>();
        grid[r][c] = color;
        stack.push(new int[]{r, c});
        while (!stack.isEmpty()) {
            int[] cur = stack.pop();
            count++;
            for (int d = 0; d < 4; d++) {
                int nr = cur[0] + DR[d];
                int nc = cur[1] + DC[d];
                if (nr >= 0 && nr < n && nc >= 0 && nc < n && grid[nr][nc] == 1) {
                    grid[nr][nc] = color;
                    stack.push(new int[]{nr, nc});
                }
            }
        }
        return count;
    }
}