import java.util.*;

class Solution {
    static List<String> sync(PageFetcher fetcher, Checkpoint checkpoint, int checkpointEvery) {
        List<String> result = new ArrayList<>();
        String cursor = checkpoint.getCursor();
        while (true) {
            Page page = fetcher.fetchPage(cursor);
            result.addAll(page.records);
            cursor = page.nextCursor;
            if (cursor == null) break;
        }
        // TODO: this only checkpoints once, after the whole sync finishes. If fetchPage
        // throws partway through, this line never runs, so nothing was ever saved and a
        // resume restarts from the very beginning -- re-delivering everything already
        // returned by the run that crashed.
        checkpoint.save(cursor);
        return result;
    }
}
