import java.util.*;

class Solution {
    static List<String> incrementalSync(List<Record> allRecords, SyncState state, long graceMillis) {
        List<String> toSync = new ArrayList<>();
        long maxSeen = state.cursor;
        long queryFrom = state.cursor;

        for (Record r : allRecords) {
            if (r.updatedAt <= queryFrom) continue;
            Long lastDelivered = state.recentlySynced.get(r.id);
            if (lastDelivered == null || lastDelivered.longValue() != r.updatedAt) {
                toSync.add(r.id);
                state.recentlySynced.put(r.id, r.updatedAt);
            }
            if (r.updatedAt > maxSeen) maxSeen = r.updatedAt;
        }

        state.cursor = maxSeen - graceMillis;
        state.recentlySynced.entrySet().removeIf(e -> e.getValue() <= state.cursor);
        return toSync;
    }
}
