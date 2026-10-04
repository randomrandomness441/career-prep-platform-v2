public class Tests {
    static int fails = 0;

    public static void main(String[] args) {
        check(2.0, 10, 1024.0);
        check(2.1, 3, 9.261);
        check(2.0, -2, 0.25);
        check(2.0, 0, 1.0);
        check(-2.0, 3, -8.0);
        check(-2.0, -3, -0.125);
        check(0.0, 5, 0.0);
        check(1.0, Integer.MIN_VALUE, 1.0);
        check(2.0, Integer.MIN_VALUE, 0.0);

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    static void check(double x, int n, double expected) {
        double actual = new Solution().myPow(x, n);
        double tol = 1e-9 * Math.max(1.0, Math.abs(expected));
        if (!(Math.abs(actual - expected) <= tol)) {
            System.out.println("FAIL myPow(" + x + ", " + n + "): expected " + expected + ", got " + actual);
            fails++;
        }
    }
}