import java.util.*;

public class Tests {
    public static void main(String[] args) {
        int fails = 0;

        fails += check(4, 2, 6, 2, "leetcode example 1");
        fails += check(6, 1, 10, 3, "leetcode example 2");
        fails += check(1, 0, 1, 1, "smallest possible input");
        fails += check(1, 0, 1000000000, 1000000000, "single element takes whole budget");
        fails += check(4, 0, 4, 1, "maxSum equals n, index at edge");
        fails += check(2, 0, 3, 2, "two elements");
        fails += check(7, 3, 8, 2, "symmetric, ramp floors at 1");
        fails += check(9, 3, 16, 3, "floor at 1 on the long side");
        fails += check(1000000, 499999, 2000000, 1001, "large n, sums near 2e6");
        fails += check(3, 2, 1000000000, 333333334, "maxSum at upper bound");

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    static int check(int n, int index, int maxSum, int expected, String label) {
        int actual = new Solution().maxValue(n, index, maxSum);
        if (actual != expected) {
            System.out.println("FAILED [" + label + "] maxValue(n=" + n + ", index=" + index
                    + ", maxSum=" + maxSum + "): expected " + expected + ", got " + actual);
            return 1;
        }
        return 0;
    }
}