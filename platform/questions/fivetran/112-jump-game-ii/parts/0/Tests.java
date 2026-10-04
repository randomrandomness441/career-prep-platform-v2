import java.util.*;

public class Tests {

    static int fails = 0;

    static void check(String name, int actual, int expected) {
        if (actual != expected) {
            System.out.println("FAIL " + name + ": expected " + expected + ", got " + actual);
            fails++;
        }
    }

    public static void main(String[] args) {
        Solution s = new Solution();

        // LeetCode example 1
        check("example1", s.jump(new int[]{2, 3, 1, 1, 4}), 2);

        // LeetCode example 2
        check("example2", s.jump(new int[]{2, 3, 0, 1, 4}), 2);

        // Trivial: already standing on the last index
        check("singleZero", s.jump(new int[]{0}), 0);
        check("singleOne", s.jump(new int[]{1}), 0);

        // Small normal case
        check("twoElements", s.jump(new int[]{1, 2}), 1);

        // Forced to walk one index at a time
        check("allOnes", s.jump(new int[]{1, 1, 1, 1}), 3);

        // Zero in the middle that must be stepped around
        check("zeroInMiddle", s.jump(new int[]{2, 2, 0, 1, 1}), 3);

        // Long jumps beat short ones
        check("longJumps", s.jump(new int[]{1, 3, 5, 2, 1}), 2);

        // One jump covers the whole array
        check("oneBigJump", s.jump(new int[]{5, 1, 1, 1, 4}), 1);

        // Constraint edge: max length, every value 1
        int[] max = new int[10000];
        Arrays.fill(max, 1);
        check("maxSize", s.jump(max), 9999);

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }
}