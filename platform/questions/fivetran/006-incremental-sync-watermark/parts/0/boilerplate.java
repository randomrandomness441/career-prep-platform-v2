import java.util.*;

class Solution {
    static List<String> incrementalSync(List<Record> allRecords, SyncState state, long graceMillis) {
        List<String> toSync = new ArrayList<>();
        long maxSeen = state.cursor;

        for (Record r : allRecords) {
            // TODO: plain "updated_at > cursor", no grace window and no memory of what
            // was delivered last. Once the cursor passes a timestamp, a record that
            // comes back with an EARLIER updatedAt (clock skew) can never pass this
            // check again -- it's dropped for good instead of being re-synced.
            if (r.updatedAt > state.cursor) {
                toSync.add(r.id);
            }
            if (r.updatedAt > maxSeen) maxSeen = r.updatedAt;
        }

        state.cursor = maxSeen;
        return toSync;
    }
}
