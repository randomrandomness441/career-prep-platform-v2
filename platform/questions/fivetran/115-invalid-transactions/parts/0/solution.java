import java.util.*;

class Solution {

    public static List<String> invalidTransactions(String[] transactions) {
        int n = transactions.length;
        String[] name = new String[n];
        String[] city = new String[n];
        int[] time = new int[n];
        int[] amount = new int[n];
        Map<String, List<Integer>> byName = new HashMap<>();

        for (int i = 0; i < n; i++) {
            String[] parts = transactions[i].split(",");
            name[i] = parts[0];
            time[i] = Integer.parseInt(parts[1]);
            amount[i] = Integer.parseInt(parts[2]);
            city[i] = parts[3];
            byName.computeIfAbsent(name[i], k -> new ArrayList<>()).add(i);
        }

        List<String> result = new ArrayList<>();
        for (int i = 0; i < n; i++) {
            if (amount[i] > 1000) {
                result.add(transactions[i]);
                continue;
            }
            boolean invalid = false;
            for (int j : byName.get(name[i])) {
                if (j != i
                        && !city[j].equals(city[i])
                        && Math.abs(time[i] - time[j]) <= 60) {
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