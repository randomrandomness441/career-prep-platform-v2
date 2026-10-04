import java.util.*;

interface PageFetcher {
    Page fetchPage(String cursor);
}

class Page {
    final List<String> records;
    final String nextCursor;
    Page(List<String> records, String nextCursor) {
        this.records = records; this.nextCursor = nextCursor;
    }
}

interface Checkpoint {
    String getCursor();
    void save(String cursor);
}

class SpyCheckpoint implements Checkpoint {
    String cursor = null;
    List<String> saveHistory = new ArrayList<>();
    public String getCursor() { return cursor; }
    public void save(String c) { cursor = c; saveHistory.add(c); }
}

// Serves a fixed list of pages by index (cursor is just the next page's index, as a
// string). Throws once -- the first time page `crashAtPage` is requested -- to simulate
// the process dying mid-sync. A resume call fetches that same page again and this time
// it succeeds, the way a restarted process re-hits the same API call.
class FixedPageFetcher implements PageFetcher {
    private final List<List<String>> pages;
    private final int crashAtPage;
    private boolean crashed = false;

    FixedPageFetcher(List<List<String>> pages, int crashAtPage) {
        this.pages = pages; this.crashAtPage = crashAtPage;
    }

    public Page fetchPage(String cursor) {
        int idx = cursor == null ? 0 : Integer.parseInt(cursor);
        if (idx == crashAtPage && !crashed) {
            crashed = true;
            throw new RuntimeException("simulated crash fetching page " + idx);
        }
        List<String> records = pages.get(idx);
        String next = (idx + 1 < pages.size()) ? String.valueOf(idx + 1) : null;
        return new Page(records, next);
    }
}

public class Tests {
    static int fails = 0;

    static void expect(boolean cond, String name) {
        if (!cond) { System.out.println("FAILED: " + name); fails++; }
    }

    public static void main(String[] args) {
        // A: no crash, just check the full set comes back in source order.
        List<List<String>> basicPages = List.of(
            List.of("a", "b"), List.of("c", "d"), List.of("e"));
        List<String> got = Solution.sync(
            new FixedPageFetcher(basicPages, -1), new SpyCheckpoint(), 2);
        expect(got.equals(List.of("a", "b", "c", "d", "e")), "basic: full set in order");

        // B: checkpoint cadence. Page sizes [4,2,1], checkpointEvery=3.
        // Page0 (4 recs) crosses the threshold by itself -> checkpoint right after it,
        // pointing at page1. Page1 alone (2 recs) doesn't cross 3 -> no checkpoint.
        // Page2 (1 rec) brings the running total since the last checkpoint to 3 -> checkpoint.
        // A correct solution never checkpoints mid-page and never resets the running
        // count except when it actually saves.
        List<List<String>> cadencePages = List.of(
            List.of("a", "b", "c", "d"), List.of("e", "f"), List.of("g"));
        SpyCheckpoint cadenceCp = new SpyCheckpoint();
        Solution.sync(new FixedPageFetcher(cadencePages, -1), cadenceCp, 3);
        expect(cadenceCp.saveHistory.equals(Arrays.asList("1", null)),
            "cadence: checkpoint fires exactly when the running count crosses N, got " + cadenceCp.saveHistory);

        // C: crash and resume. 5 pages of 4 records each, checkpointEvery == page size,
        // so a checkpoint lands after every successful page. The fetcher throws once,
        // fetching page 3 -- simulating the process dying after pages 0-2 are done.
        List<List<String>> bigPages = new ArrayList<>();
        for (int p = 0; p < 5; p++) {
            List<String> page = new ArrayList<>();
            for (int r = 0; r < 4; r++) page.add("p" + p + "r" + r);
            bigPages.add(page);
        }
        FixedPageFetcher crashFetcher = new FixedPageFetcher(bigPages, 3);
        SpyCheckpoint crashCp = new SpyCheckpoint();
        boolean threw = false;
        try {
            Solution.sync(crashFetcher, crashCp, 4);
        } catch (RuntimeException expected) {
            threw = true;
        }
        expect(threw, "crash: exception from fetchPage propagates out of sync");
        expect("3".equals(crashCp.getCursor()),
            "crash: checkpoint reflects the 3 pages committed before the crash, got " + crashCp.getCursor());

        List<String> resumed = Solution.sync(crashFetcher, crashCp, 4);
        List<String> expectedResume = new ArrayList<>();
        for (int r = 0; r < 4; r++) expectedResume.add("p3r" + r);
        for (int r = 0; r < 4; r++) expectedResume.add("p4r" + r);
        expect(resumed.equals(expectedResume),
            "resume: refetches only the uncommitted pages (3 and 4), no duplicates and nothing skipped, got " + resumed);

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
