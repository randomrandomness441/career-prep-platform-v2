import java.util.*;

class Solution {
    public int ladderLength(String beginWord, String endWord, List<String> wordList) {
        Set<String> dict = new HashSet<>(wordList);
        if (!dict.contains(endWord)) {
            return 0;
        }
        dict.add(beginWord);

        Map<String, List<String>> patternToWords = new HashMap<>();
        for (String word : dict) {
            for (int i = 0; i < word.length(); i++) {
                String pattern = word.substring(0, i) + "*" + word.substring(i + 1);
                patternToWords.computeIfAbsent(pattern, k -> new ArrayList<>()).add(word);
            }
        }

        Queue<String> queue = new LinkedList<>();
        Set<String> visited = new HashSet<>();
        queue.offer(beginWord);
        visited.add(beginWord);

        int steps = 1;
        while (!queue.isEmpty()) {
            int levelSize = queue.size();
            for (int i = 0; i < levelSize; i++) {
                String current = queue.poll();
                if (current.equals(endWord)) {
                    return steps;
                }
                for (int j = 0; j < current.length(); j++) {
                    String pattern = current.substring(0, j) + "*" + current.substring(j + 1);
                    for (String next : patternToWords.getOrDefault(pattern, Collections.emptyList())) {
                        if (visited.add(next)) {
                            queue.offer(next);
                        }
                    }
                }
            }
            steps++;
        }
        return 0;
    }
}