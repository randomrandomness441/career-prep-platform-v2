import java.util.*;

class Solution {
    static Delta reconcile(List<Record> source, List<Record> target) {
        // Index each side by id once -- O(n) and O(m) -- so every lookup after that
        // is O(1) instead of a linear scan. A Map's put() naturally keeps the last
        // value for a repeated key, which is exactly "last occurrence wins".
        Map<String, String> targetById = new HashMap<>();
        for (Record t : target) targetById.put(t.id, t.checksum);

        Map<String, String> sourceById = new HashMap<>();
        for (Record s : source) sourceById.put(s.id, s.checksum);

        List<String> toInsert = new ArrayList<>();
        List<String> toUpdate = new ArrayList<>();
        for (Map.Entry<String, String> e : sourceById.entrySet()) {
            String prev = targetById.get(e.getKey());
            if (prev == null) {
                toInsert.add(e.getKey());
            } else if (!prev.equals(e.getValue())) {
                toUpdate.add(e.getKey());
            }
            // prev != null && prev.equals(checksum) -> unchanged, not reported
        }

        List<String> toDelete = new ArrayList<>();
        for (String id : targetById.keySet()) {
            if (!sourceById.containsKey(id)) toDelete.add(id);
        }

        return new Delta(toInsert, toUpdate, toDelete);
    }
}
