public class Tests {
    static int fails = 0;

    static void check(int[][] matrix, int target, boolean expected, String label) {
        boolean got = new Solution().searchMatrix(matrix, target);
        if (got != expected) {
            System.out.println("FAIL " + label + ": target=" + target
                    + " expected=" + expected + " got=" + got);
            fails++;
        }
    }

    public static void main(String[] args) {
        int[][] classic = {{1, 3, 5, 7}, {10, 11, 16, 20}, {23, 30, 34, 60}};

        // Classic present and absent cases from the problem statement.
        check(classic, 3, true, "classic-present");
        check(classic, 13, false, "classic-absent-gap");

        // Target equals the first element of a row past row 0.
        check(classic, 10, true, "first-element-of-second-row");

        // Targets below and above every value in the matrix.
        check(classic, -10000, false, "below-all-values");
        check(classic, 10000, false, "above-all-values");

        // Smallest matrix the constraints allow: 1 x 1.
        check(new int[][]{{1}}, 1, true, "1x1-present");
        check(new int[][]{{1}}, 0, false, "1x1-absent");

        // Single column matrix, n = 1.
        check(new int[][]{{1}, {3}, {5}}, 3, true, "single-column-present");

        // Negative values spanning several rows.
        check(new int[][]{{-8, -5, -2}, {0, 3, 7}, {10, 20, 30}}, 0, true, "negative-values-row-start");

        // Last element of the whole matrix.
        check(classic, 60, true, "last-element-overall");

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }
}