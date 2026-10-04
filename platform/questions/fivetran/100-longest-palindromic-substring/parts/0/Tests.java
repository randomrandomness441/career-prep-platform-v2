import java.util.*;

public class Tests {
    static int fails = 0;

    public static void main(String[] args) {
        check("babad", 3, null);
        check("cbbd", 2, "bb");
        check("a", 1, "a");
        check("ac", 1, null);
        check("abcba", 5, "abcba");
        check("aaaa", 4, "aaaa");
        check("abb", 2, "bb");
        check("abacdfgdcaba", 3, null);
        check("abcdefghij", 1, null);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 1000; i++) {
            sb.append('x');
        }
        check(sb.toString(), 1000, sb.toString());

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    static void check(String s, int wantLen, String exact) {
        String got = new Solution().longestPalindrome(s);
        boolean ok = got != null && got.length() == wantLen && isPalindrome(got) && s.contains(got);
        if (ok && exact != null) {
            ok = got.equals(exact);
        }
        if (!ok) {
            fails++;
            System.out.println("FAIL input=" + preview(s) + " expectedLen=" + wantLen
                    + (exact == null ? "" : " exact=" + preview(exact))
                    + " got=" + (got == null ? "null" : preview(got)));
        }
    }

    static boolean isPalindrome(String t) {
        int i = 0;
        int j = t.length() - 1;
        while (i < j) {
            if (t.charAt(i) != t.charAt(j)) {
                return false;
            }
            i++;
            j--;
        }
        return true;
    }

    static String preview(String t) {
        if (t.length() <= 20) {
            return "\"" + t + "\"";
        }
        return "\"" + t.substring(0, 20) + "...\"";
    }
}