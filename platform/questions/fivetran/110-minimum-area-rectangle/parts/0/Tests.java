import java.util.*;

public class Tests {
    public static void main(String[] args) {
        int fails = 0;

        // 1: a single point can never form a rectangle
        fails += check(1, new int[][]{{0, 0}}, 0, "single point");

        // 2: two points cannot form a rectangle
        fails += check(2, new int[][]{{0, 0}, {7, 9}}, 0, "two points only");

        // 3: LeetCode example 1, extra point sits inside the square
        fails += check(3, new int[][]{{1, 1}, {1, 3}, {3, 1}, {3, 3}, {2, 2}}, 4, "example 1");

        // 4: LeetCode example 2, tighter rectangle among two candidates
        fails += check(4, new int[][]{{1, 1}, {1, 3}, {3, 1}, {3, 3}, {4, 1}, {4, 3}}, 2, "example 2");

        // 5: smallest possible rectangle, sides of length 1
        fails += check(5, new int[][]{{0, 0}, {0, 1}, {1, 0}, {1, 1}}, 1, "unit rectangle");

        // 6: all points share one x, so no axis-aligned rectangle exists
        fails += check(6, new int[][]{{4, 0}, {4, 1}, {4, 2}, {4, 5}}, 0, "collinear on one column");

        // 7: extra point sitting on a corner column must not hide the rectangle
        fails += check(7, new int[][]{{1, 1}, {1, 3}, {3, 1}, {3, 3}, {3, 2}}, 4, "interior point on a column");

        // 8: the minimum rectangle spans columns 0 and 1 even though column 1
        //    also holds a middle point at y = 1
        fails += check(8, new int[][]{{0, 0}, {0, 2}, {1, 0}, {1, 1}, {1, 2}, {2, 0}, {2, 2}}, 2, "blocked middle column");

        // 9: boundary coordinates from the constraint 0 <= x, y <= 4 * 10^4
        fails += check(9, new int[][]{{0, 0}, {0, 40000}, {40000, 0}, {40000, 40000}}, 1600000000, "extreme coordinates");

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    private static int check(int id, int[][] points, int expected, String label) {
        int got = new Solution().minAreaRect(points);
        if (got != expected) {
            System.out.println("test " + id + " FAILED (" + label + "): expected " + expected + ", got " + got);
            return 1;
        }
        return 0;
    }
}