import java.util.*;

public class Tests {
    public static void main(String[] args) {
        int fails = 0;

        fails += check("abc", "pqr", "apbqcr");
        fails += check("ab", "pqrs", "apbqrs");
        fails += check("abcd", "pq", "apbqcd");
        fails += check("a", "b", "ab");
        fails += check("ab", "c", "acb");
        fails += check("x", "yz", "xyz");
        fails += check(repeat('a', 100), repeat('b', 100), repeatAB(100));
        fails += check("a", repeat('z', 100), "a" + repeat('z', 100));

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    static int check(String w1, String w2, String expected) {
        String actual = new Solution().mergeAlternately(w1, w2);
        if (!expected.equals(actual)) {
            System.out.println("FAIL mergeAlternately(len1=" + w1.length()
                    + ", len2=" + w2.length() + "): expected \"" + expected
                    + "\", got \"" + actual + "\"");
            return 1;
        }
        return 0;
    }

    static String repeat(char c, int n) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < n; i++) sb.append(c);
        return sb.toString();
    }

    static String repeatAB(int n) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < n; i++) sb.append("ab");
        return sb.toString();
    }
}