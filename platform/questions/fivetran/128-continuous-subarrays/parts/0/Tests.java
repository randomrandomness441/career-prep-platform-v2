import java.util.*;

public class Tests {
    static int fails = 0;

    static void check(String name, Solution sol, int[] nums, long expected) {
        long actual = sol.continuousSubarrays(nums);
        if (actual != expected) {
            System.out.println("FAIL " + name + ": expected " + expected + ", got " + actual);
            fails++;
        }
    }

    public static void main(String[] args) {
        Solution sol = new Solution();

        check("example 1 [5,4,2,4]", sol, new int[]{5, 4, 2, 4}, 8L);
        check("example 2 [1,2,3]", sol, new int[]{1, 2, 3}, 6L);
        check("single element", sol, new int[]{42}, 1L);
        check("all equal", sol, new int[]{1, 1, 1, 1}, 10L);
        check("no two elements combine", sol, new int[]{1, 5, 9, 13}, 4L);
        check("zigzag with step 2", sol, new int[]{1, 3, 5}, 5L);
        check("ramp with step 1", sol, new int[]{1, 2, 3, 4, 5}, 15L);
        check("mixed [8,10,10,9,12]", sol, new int[]{8, 10, 10, 9, 12}, 11L);
        check("max values", sol, new int[]{1000000000, 999999999, 999999998}, 6L);

        int[] big = new int[100000];
        Arrays.fill(big, 7);
        check("overflow n=100000 all equal", sol, big, 5000050000L);

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }
}