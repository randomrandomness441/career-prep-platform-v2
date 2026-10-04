import java.util.*;

public class Tests {
    public static void main(String[] args) {
        int fails = 0;

        // LeetCode example 1
        fails += check(1, "hit", "cog",
                Arrays.asList("hot", "dot", "dog", "lot", "log", "cog"), 5);

        // LeetCode example 2: endWord is missing from wordList
        fails += check(2, "hit", "cog",
                Arrays.asList("hot", "dot", "dog", "lot", "log"), 0);

        // single-letter words, direct one-step transformation
        fails += check(3, "a", "c",
                Arrays.asList("a", "b", "c"), 2);

        // endWord is present but unreachable
        fails += check(4, "hit", "cog",
                Arrays.asList("cog"), 0);

        // minimal wordList of size 1, exactly one step away
        fails += check(5, "hot", "dot",
                Arrays.asList("dot"), 2);

        // three-step chain, beginWord not in wordList
        fails += check(6, "hot", "cog",
                Arrays.asList("dot", "dog", "cog"), 4);

        // two routes exist, the shorter one must win
        fails += check(7, "hit", "cog",
                Arrays.asList("hot", "dot", "dog", "lot", "log", "cog", "cot"), 4);

        // both endpoints in wordList but no intermediate exists
        fails += check(8, "hot", "dog",
                Arrays.asList("hot", "dog"), 0);

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        } else {
            System.out.println("all checks passed");
        }
    }

    private static int check(int id, String beginWord, String endWord,
                             List<String> wordList, int expected) {
        int got = new Solution().ladderLength(beginWord, endWord, wordList);
        if (got != expected) {
            System.out.println("test " + id + " FAILED: beginWord=" + beginWord
                    + ", endWord=" + endWord + ", wordList=" + wordList
                    + ", expected=" + expected + ", got=" + got);
            return 1;
        }
        return 0;
    }
}