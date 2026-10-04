import java.util.*;

public class Tests {

    static int fails = 0;

    static void check(String name, String expected, String actual) {
        if (expected.equals(actual)) {
            System.out.println("PASS: " + name);
        } else {
            fails++;
            System.out.println("FAIL: " + name);
            System.out.println("  expected: " + expected);
            System.out.println("  actual:   " + actual);
        }
    }

    public static void main(String[] args) {
        Solution sol = new Solution();

        // trivial: single character
        check("single char, one row", "A", sol.convert("A", 1));

        // trivial: one row means no movement at all
        check("one row keeps order", "AB", sol.convert("AB", 1));

        // classic examples from the problem statement
        check("classic numRows=3", "PAHNAPLSIIGYIR", sol.convert("PAYPALISHIRING", 3));
        check("classic numRows=4", "PINALSIGYAHRPI", sol.convert("PAYPALISHIRING", 4));

        // small normal case
        check("two rows", "ACBD", sol.convert("ABCD", 2));

        // edge: numRows equals the length of s
        check("rows equal length", "ABCDE", sol.convert("ABCDE", 5));

        // edge: numRows at its maximum, larger than the string
        check("rows exceed length", "ABC", sol.convert("ABC", 1000));

        // edge: constraints allow ',' and '.' in s
        check("punctuation chars", "ABC,.", sol.convert("A,B.C", 2));

        // edge: max length input with numRows = 2, so evens then odds
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 1000; i++) {
            sb.append((char) ('a' + (i % 26)));
        }
        String big = sb.toString();
        StringBuilder expectedBig = new StringBuilder();
        for (int i = 0; i < 1000; i += 2) {
            expectedBig.append(big.charAt(i));
        }
        for (int i = 1; i < 1000; i += 2) {
            expectedBig.append(big.charAt(i));
        }
        check("length 1000, numRows=2", expectedBig.toString(), sol.convert(big, 2));

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }
}