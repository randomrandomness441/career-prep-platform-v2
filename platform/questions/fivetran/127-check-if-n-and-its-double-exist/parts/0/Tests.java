import java.util.Arrays;

public class Tests {
    public static void main(String[] args) {
        int fails = 0;

        // LeetCode example 1: 10 == 2 * 5
        fails += check("example1", new int[]{10, 2, 5, 3}, true);
        // LeetCode example 2: 14 == 2 * 7
        fails += check("example2", new int[]{7, 1, 14, 11}, true);
        // LeetCode example 3: no element is the double of another
        fails += check("example3", new int[]{3, 1, 7, 11}, false);
        // two zeros form a valid pair at distinct indices
        fails += check("two-zeros", new int[]{0, 0}, true);
        // a single zero must NOT count as its own double
        fails += check("single-zero", new int[]{-10, 0}, false);
        // mixed negatives and positives, no pair
        fails += check("mixed-no-pair", new int[]{-2, 0, 10, -19, 4, 6, -8}, false);
        // negative pair: -4 == 2 * -2
        fails += check("negative-pair", new int[]{-2, -4}, true);
        // values near the constraint bounds
        fails += check("bounds-hit", new int[]{500, -1000, 250}, true);
        fails += check("bounds-extremes-miss", new int[]{1000, -1000}, false);

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    private static int check(String name, int[] arr, boolean expected) {
        boolean actual = new Solution().checkIfExist(arr);
        if (actual != expected) {
            System.out.println("FAIL " + name + " input=" + Arrays.toString(arr)
                    + " expected=" + expected + " actual=" + actual);
            return 1;
        }
        return 0;
    }
}