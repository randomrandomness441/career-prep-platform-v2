import java.util.*;

public class Tests {
    static int fails = 0;

    static void check(String start, String target, boolean expected) {
        boolean actual = new Solution().canChange(start, target);
        if (actual != expected) {
            System.out.println("FAIL: canChange(\"" + start + "\", \"" + target
                    + "\") expected " + expected + " but got " + actual);
            fails++;
        }
    }

    public static void main(String[] args) {
        // LeetCode example 1: L slides left, both Rs slide right
        check("_L__R__R_", "L______RR", true);
        // LeetCode example 2: pieces would need to swap order
        check("R_L_", "__LR", false);
        // LeetCode example 3: R cannot slide left
        check("_R", "R_", false);
        // trivial: single blank, nothing to move
        check("_", "_", true);
        // single piece already in place
        check("L", "L", true);
        // R slides right once
        check("R_", "_R", true);
        // L cannot slide right
        check("L_", "_L", false);
        // L slides left once
        check("_L", "L_", true);
        // same piece order but L would need to move right
        check("L__R", "_L_R", false);
        // R slides right past blanks, L stays put
        check("R__L", "__RL", true);

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }
}