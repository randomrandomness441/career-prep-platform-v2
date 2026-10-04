import java.util.*;

public class Tests {
    public static void main(String[] args) {
        int fails = 0;

        fails += check(new int[]{7}, 0, "single bar, no water");
        fails += check(new int[]{0,1,0,2,1,0,1,3,2,1,2,1}, 6, "example 1");
        fails += check(new int[]{4,2,0,3,2,5}, 9, "example 2");
        fails += check(new int[]{2,0,2}, 2, "small valley");
        fails += check(new int[]{3,3,3}, 0, "flat bars");
        fails += check(new int[]{1,2,3,4,5}, 0, "strictly rising");
        fails += check(new int[]{5,4,3,2,1}, 0, "strictly falling");
        fails += check(new int[]{100000,0,100000}, 100000, "max height from constraints");

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    static int check(int[] height, int expected, String label) {
        int actual = new Solution().trap(height);
        if (actual != expected) {
            System.out.println("FAIL [" + label + "] expected=" + expected + " actual=" + actual);
            return 1;
        }
        return 0;
    }
}