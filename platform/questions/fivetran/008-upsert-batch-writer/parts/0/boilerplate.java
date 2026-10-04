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
                if (r.parentId == null || committed.contains(r.parentId) || !byId.containsKey(r.parentId)) {
                    sorted.add(r.id);
                    committed.add(r.id);
                    it.remove();
                    progress = true;
                }
            }
            // TODO: if a whole pass makes no progress, whatever's left in `remaining`
            // forms a dependency cycle -- but this just stops the loop instead of telling
            // the caller. Those rows silently vanish from the output instead of the sync
            // ever finding out it can't safely write them.
            if (!progress) break;
        }

        List<List<String>> batches = new ArrayList<>();
        for (int i = 0; i < sorted.size(); i += batchSize) {
            batches.add(new ArrayList<>(sorted.subList(i, Math.min(i + batchSize, sorted.size()))));
        }
        return batches;
    }
}
