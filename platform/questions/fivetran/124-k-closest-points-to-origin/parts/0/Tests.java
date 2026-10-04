import java.util.*;

public class Tests {
    static int fails = 0;

    public static void main(String[] args) {
        check(new int[][]{{1, 3}, {-2, 2}}, 1, new int[][]{{-2, 2}});
        check(new int[][]{{3, 3}, {5, -1}, {-2, 4}}, 2, new int[][]{{3, 3}, {-2, 4}});
        check(new int[][]{{0, 0}}, 1, new int[][]{{0, 0}});
        check(new int[][]{{1, 1}, {2, 2}, {3, 3}}, 3, new int[][]{{1, 1}, {2, 2}, {3, 3}});
        check(new int[][]{{-5, 4}, {-3, 2}, {-1, 1}, {2, -6}}, 2, new int[][]{{-1, 1}, {-3, 2}});
        check(new int[][]{{10000, 10000}, {9999, 9998}, {3, 4}}, 2, new int[][]{{3, 4}, {9999, 9998}});
        check(new int[][]{{0, 0}, {5, 5}, {-1, -1}}, 1, new int[][]{{0, 0}});
        check(new int[][]{{1, 2}, {1, 2}, {3, 4}}, 2, new int[][]{{1, 2}, {1, 2}});

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    static void check(int[][] points, int k, int[][] expected) {
        int[][] actual = new Solution().kClosest(points, k);
        if (actual == null || actual.length != k) {
            System.out.println("FAIL: points=" + Arrays.deepToString(points) + " k=" + k
                    + " expected " + k + " row(s), got "
                    + (actual == null ? "null" : String.valueOf(actual.length)));
            fails++;
            return;
        }
        for (int[] row : actual) {
            if (row == null || row.length != 2) {
                System.out.println("FAIL: points=" + Arrays.deepToString(points) + " k=" + k
                        + " malformed row " + Arrays.toString(row));
                fails++;
                return;
            }
        }
        int[][] got = canonical(actual);
        int[][] want = canonical(expected);
        for (int i = 0; i < got.length; i++) {
            if (got[i][0] != want[i][0] || got[i][1] != want[i][1]) {
                System.out.println("FAIL: points=" + Arrays.deepToString(points) + " k=" + k
                        + " row " + i + " expected [" + want[i][0] + "," + want[i][1] + "]"
                        + " got [" + got[i][0] + "," + got[i][1] + "]");
                fails++;
                return;
            }
        }
    }

    static int[][] canonical(int[][] rows) {
        int[][] copy = rows.clone();
        Arrays.sort(copy, (a, b) -> a[0] != b[0]
                ? Integer.compare(a[0], b[0])
                : Integer.compare(a[1], b[1]));
        return copy;
    }
}