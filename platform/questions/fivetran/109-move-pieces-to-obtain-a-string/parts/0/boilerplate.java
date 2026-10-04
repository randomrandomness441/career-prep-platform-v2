class Solution {
    public boolean canChange(String start, String target) {
        // Pieces cannot pass each other, so the sequence of Ls and Rs
        // must match once blanks are ignored.
        StringBuilder a = new StringBuilder();
        StringBuilder b = new StringBuilder();
        for (char c : start.toCharArray()) {
            if (c != '_') a.append(c);
        }
        for (char c : target.toCharArray()) {
            if (c != '_') b.append(c);
        }
        return a.toString().equals(b.toString());
    }
}