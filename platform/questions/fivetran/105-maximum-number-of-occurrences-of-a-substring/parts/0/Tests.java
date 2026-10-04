import java.util.*;

public class Tests {
    static int fails = 0;

    static void check(String s, int maxLetters, int minSize, int maxSize, int expected) {
        int got = new Solution().maxFreq(s, maxLetters, minSize, maxSize);
        if (got != expected) {
            fails++;
            System.out.println("FAIL: maxFreq(s=\"" + s + "\", maxLetters=" + maxLetters
                    + ", minSize=" + minSize + ", maxSize=" + maxSize
                    + ") expected " + expected + " but got " + got);
        }
    }

    public static void main(String[] args) {
        // LeetCode example 1: "aab" appears twice with 2 unique letters.
        check("aababcaab", 2, 3, 4, 2);
        // LeetCode example 2: "aaa" appears twice.
        check("aaaa", 1, 3, 3, 2);
        // LeetCode example 3: "ab" appears 3 times.
        check("aabcabcab", 2, 2, 3, 3);
        // LeetCode example 4: no window has <= 2 unique letters.
        check("abcde", 2, 3, 3, 0);
        // Trivial case: single character, minSize == maxSize == 1.
        check("a", 1, 1, 1, 1);
        // minSize < maxSize: "aaa" appears 3 times and beats any size-4 window.
        check("aaaaa", 1, 3, 4, 3);
        // Every size-2 window has 2 unique letters but maxLetters is 1.
        check("abcabc", 1, 2, 2, 0);
        // minSize of 1: both "a" and "b" repeat twice.
        check("abab", 2, 1, 2, 2);
        // maxSize far above minSize: "aa" repeats 5 times.
        check("aaaaaa", 1, 2, 5, 5);
        // Mixed letters: "is", "ss", and "si" each appear twice.
        check("mississippi", 2, 2, 3, 2);

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }
}