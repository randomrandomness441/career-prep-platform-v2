import java.util.*;

class Solution {
    static List<String> sync(PageFetcher fetcher, Checkpoint checkpoint, int checkpointEvery) {
        List<String> result = new ArrayList<>();
        String cursor = checkpoint.getCursor();
        int sinceCheckpoint = 0;
        while (true) {
            Page page = fetcher.fetchPage(cursor);
            result.addAll(page.records);
            sinceCheckpoint += page.records.size();
            cursor = page.nextCursor;
            // Only ever checkpoint right after a full page, once enough records have
            // accumulated since the last save. Never mid-page.
            if (sinceCheckpoint >= checkpointEvery) {
                checkpoint.save(cursor);
                sinceCheckpoint = 0;
            }
            if (cursor == null) break;
        }
        return result;
    }
}
