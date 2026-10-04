import java.util.*;

public class Tests {

    static int fails = 0;

    public static void main(String[] args) {
        check(1, new int[]{0, 1, 3, 50, 75}, 0, 99,
                Arrays.asList(
                        Arrays.asList(2, 2),
                        Arrays.asList(4, 49),
                        Arrays.asList(51, 74),
                        Arrays.asList(76, 99)));

        check(2, new int[]{-1}, -2, -1,
                Arrays.asList(Arrays.asList(-2, -2)));

        check(3, new int[]{}, 1, 1,
                Arrays.asList(Arrays.asList(1, 1)));

        check(4, new int[]{}, -3, -1,
                Arrays.asList(Arrays.asList(-3, -1)));

        check(5, new int[]{1, 2, 3}, 1, 3,
                Collections.emptyList());

        check(6, new int[]{-1}, -1, -1,
                Collections.emptyList());

        check(7, new int[]{1, 3}, 0, 4,
                Arrays.asList(
                        Arrays.asList(0, 0),
                        Arrays.asList(2, 2),
                        Arrays.asList(4, 4)));

        check(8, new int[]{0, 999999998}, 0, 1000000000,
                Arrays.asList(
                        Arrays.asList(1, 999999997),
                        Arrays.asList(999999999, 1000000000)));

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        } else {
            System.out.println("all checks passed");
        }
    }

    static void check(int id, int[] nums, int lower, int upper, List<List<Integer>> expected) {
        List<List<Integer>> actual = new Solution().findMissingRanges(nums, lower, upper);
        if (!expected.equals(actual)) {
            fails++;
            System.out.println("test " + id + " FAILED: expected " + expected + " but got " + actual);
        }
    }
}