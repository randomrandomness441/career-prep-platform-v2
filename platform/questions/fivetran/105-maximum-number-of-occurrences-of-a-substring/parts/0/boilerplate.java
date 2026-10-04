import java.util.*;

class Solution {
    public int maxFreq(String s, int maxLetters, int minSize, int maxSize) {
        // Any valid substring of length minSize sits inside some valid window of
        // length maxSize, so counting the maxSize windows should be enough.
        Map<String, Integer> count = new HashMap<>();
        int best = 0;
        for (int i = 0; i + maxSize <= s.length(); i++) {
            String sub = s.substring(i, i + maxSize);
            if (uniqueLetters(sub) <= maxLetters) {
                int c = count.merge(sub, 1, Integer::sum);
                if (c > best) best = c;
            }
        }
        return best;
    }

    private int uniqueLetters(String t) {
        boolean[] seen = new boolean[26];
        int distinct = 0;
        for (char ch : t.toCharArray()) {
            if (!seen[ch - 'a']) {
                seen[ch - 'a'] = true;
                distinct++;
            }
        }
        return distinct;
    }
}