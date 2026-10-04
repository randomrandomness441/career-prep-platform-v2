import java.util.*;

class Solution {
    public int[] deckRevealedIncreasing(int[] deck) {
        Arrays.sort(deck);
        int n = deck.length;
        Deque<Integer> indices = new ArrayDeque<>();
        for (int i = 0; i < n; i++) {
            indices.addLast(i);
        }
        int[] res = new int[n];
        for (int card : deck) {
            if (indices.size() > 1) {
                indices.addLast(indices.pollFirst());
            }
            res[indices.pollFirst()] = card;
        }
        return res;
    }
}