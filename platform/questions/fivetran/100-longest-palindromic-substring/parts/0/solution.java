class Solution {
    public String longestPalindrome(String s) {
        if (s == null || s.length() < 2) {
            return s;
        }
        int bestStart = 0;
        int bestLen = 1;
        for (int i = 0; i < s.length(); i++) {
            int oddLen = expand(s, i, i);
            int evenLen = expand(s, i, i + 1);
            int cur = Math.max(oddLen, evenLen);
            if (cur > bestLen) {
                bestLen = cur;
                bestStart = i - (cur - 1) / 2;
            }
        }
        return s.substring(bestStart, bestStart + bestLen);
    }

    private int expand(String s, int l, int r) {
        while (l >= 0 && r < s.length() && s.charAt(l) == s.charAt(r)) {
            l--;
            r++;
        }
        return r - l - 1;
    }
}