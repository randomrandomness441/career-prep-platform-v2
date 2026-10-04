class Solution {
    public String convert(String s, int numRows) {
        if (numRows == 1 || numRows >= s.length()) {
            return s;
        }
        int n = s.length();
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < numRows; i++) {
            int stepDown = 2 * (numRows - 1 - i);
            int stepUp = 2 * i;
            int j = i;
            while (j < n) {
                sb.append(s.charAt(j));
                j += stepDown;
                if (j >= n) break;
                sb.append(s.charAt(j));
                j += stepUp;
            }
        }
        return sb.toString();
    }
}