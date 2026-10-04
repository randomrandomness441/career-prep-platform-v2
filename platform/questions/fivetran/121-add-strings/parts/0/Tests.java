public class Tests {
    static int fails = 0;

    static void check(String name, String expected, String actual) {
        if (!expected.equals(actual)) {
            System.out.println("FAIL " + name + ": expected " + expected + " actual " + actual);
            fails++;
        }
    }

    public static void main(String[] args) {
        Solution s = new Solution();

        check("small normal case", "134", s.addStrings("11", "123"));
        check("mid-size with carries", "533", s.addStrings("456", "77"));
        check("both zero", "0", s.addStrings("0", "0"));
        check("single digits carry out", "10", s.addStrings("1", "9"));
        check("all nines carry out", "100", s.addStrings("99", "1"));
        check("beyond long range", "10000000000000000000", s.addStrings("9999999999999999999", "1"));
        check("unequal lengths", "1111111110", s.addStrings("123456789", "987654321"));
        check("add zero keeps value", "1000", s.addStrings("1000", "0"));

        StringBuilder nines = new StringBuilder();
        for (int i = 0; i < 10000; i++) nines.append('9');
        StringBuilder want = new StringBuilder("1");
        for (int i = 0; i < 10000; i++) want.append('0');
        check("max length all nines plus one", want.toString(), s.addStrings(nines.toString(), "1"));

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }
}