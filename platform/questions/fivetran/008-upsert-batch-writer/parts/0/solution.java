import java.util.*;

class Solution {
    static List<List<String>> buildBatches(List<Row> rows, int batchSize) {
        Map<String, Row> byId = new HashMap<>();
        for (Row r : rows) byId.put(r.id, r);

        List<Row> remaining = new ArrayList<>(rows);
        Set<String> committed = new HashSet<>();
        List<String> sorted = new ArrayList<>();

        while (!remaining.isEmpty()) {
            boolean progress = false;
            for (Iterator<Row> it = remaining.iterator(); it.hasNext(); ) {
                Row r = it.next();
                // A parent not present in this batch at all imposes no constraint --
                // it's already in the destination table.
                if (r.parentId == null || committed.contains(r.parentId) || !byId.containsKey(r.parentId)) {
                    sorted.add(r.id);
                    committed.add(r.id);
                    it.remove();
                    progress = true;
                }
            }
            if (!progress) {
                List<String> stuck = new ArrayList<>();
                for (Row r : remaining) stuck.add(r.id);
                throw new IllegalArgumentException("dependency cycle among: " + stuck);
            }
        }

        List<List<String>> batches = new ArrayList<>();
        for (int i = 0; i < sorted.size(); i += batchSize) {
            batches.add(new ArrayList<>(sorted.subList(i, Math.min(i + batchSize, sorted.size()))));
        }
        return batches;
    }
}
