import java.util.*;

class Solution {

    public static List<String> invalidTransactions(String[] transactions) {
        int n = transactions.length;
        int[] time = new int[n];
        Map<String, List<Integer>> byName = new HashMap<>();

        for (int i = 0; i < n; i++) {
            String[] parts = transactions[i].split(",");
            time[i] = Integer.parseInt(parts[1]);
            byName.computeIfAbsent(parts[0], k -> new ArrayList<>()).add(i);
        }

        List<String> result = new ArrayList<>();
        for (int i = 0; i < n; i++) {
            String[] parts = transactions[i].split(",");
            int amount = Integer.parseInt(parts[2]);
            boolean invalid = amount > 1000;
            for (int j : byName.get(parts[0])) {
                if (j != i && Math.abs(time[i] - time[j]) <= 60) {
                    invalid = true;
                    break;
                }
            }
            if (invalid) {
                result.add(transactions[i]);
            }
        }
        return result;
    }
}