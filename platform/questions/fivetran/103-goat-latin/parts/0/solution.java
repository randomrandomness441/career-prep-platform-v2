class Solution {
    public String toGoatLatin(String sentence) {
        String[] words = sentence.split(" ");
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < words.length; i++) {
            if (i > 0) {
                result.append(' ');
            }
            char first = words[i].charAt(0);
            StringBuilder word = new StringBuilder();
            if (isVowel(first)) {
                word.append(words[i]);
            } else {
                word.append(words[i].substring(1)).append(first);
            }
            word.append("ma");
            for (int j = 0; j <= i; j++) {
                word.append('a');
            }
            result.append(word);
        }
        return result.toString();
    }

    private boolean isVowel(char c) {
        return "aeiouAEIOU".indexOf(c) >= 0;
    }
}