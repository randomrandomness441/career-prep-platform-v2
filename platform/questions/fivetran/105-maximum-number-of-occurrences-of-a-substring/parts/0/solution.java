import java.util.*;

class Solution {
    public int maxFreq(String s, int maxLetters, int minSize, int maxSize) {
        // If a substring of length L >= minSize appears k times, then some
        // length-minSize piece of it appears at least k times too. So only
        // windows of exactly minSize matter and maxSize can be ignored.
        int n = s.length();
        int[] freq = new int[26];
        int distinct = 0;
        Map<String, Integer> count = new HashMap<>();
        int best = 0;
        for (int i = 0; i < n; i++) {
            int in = s.charAt(i) - 'a';
            if (freq[in]++ == 0) distinct++;
            if (i >= minSize) {
                int out = s.charAt(i - minSize) - 'a';
                if (--freq[out] == 0) distinct--;
            }
            if (i >= minSize - 1 && distinct <= maxLetters) {
                String sub = s.substring(i - minSize + 1, i + 1);
                int c = count.merge(sub, 1, Integer::sum);
                if (c > best) best = c;
            }
        }
        return best;
    }
}