class Solution {
    public String longestPalindrome(String s) {
        if (s == null || s.length() < 2) {
            return s;
        }
        int bestStart = 0;
        int bestLen = 1;
        for (int i = 0; i < s.length(); i++) {
            int l = i - 1;
            int r = i + 1;
            while (l >= 0 && r < s.length() && s.charAt(l) == s.charAt(r)) {
                l--;
                r++;
            }
            int cur = r - l - 1;
            if (cur > bestLen) {
                bestLen = cur;
                bestStart = l + 1;
            }
        }
        return s.substring(bestStart, bestStart + bestLen);
    }
}