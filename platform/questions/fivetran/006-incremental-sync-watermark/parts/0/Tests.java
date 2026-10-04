import java.util.*;

class Record {
    final String id;
    final long updatedAt;
    final boolean deleted;
    Record(String id, long updatedAt, boolean deleted) {
        this.id = id; this.updatedAt = updatedAt; this.deleted = deleted;
    }
}

class SyncState {
    long cursor;
    Map<String, Long> recentlySynced;
    SyncState(long cursor, Map<String, Long> recentlySynced) {
        this.cursor = cursor; this.recentlySynced = recentlySynced;
    }
}

public class Tests {
    static int fails = 0;

    static void expectEq(List<String> got, List<String> want, String name) {
        if (!got.equals(want)) {
            System.out.printf("FAILED %s: got %s want %s%n", name, got, want);
            fails++;
        }
    }

    public static void main(String[] args) {
        // Clock skew: record 'a' is re-synced at t=100, then reappears at t=90 -- an
        // earlier stamp than what's already been delivered for it. It must still be
        // re-synced, not treated as "already seen" or filtered out by the watermark.
        SyncState state = new SyncState(0L, new HashMap<>());
        List<String> run1 = Solution.incrementalSync(
            List.of(new Record("a", 100, false), new Record("b", 80, false)), state, 50);
        expectEq(run1, List.of("a", "b"), "run1: both records inside the window");

        List<String> run2 = Solution.incrementalSync(
            List.of(new Record("a", 90, false), new Record("b", 80, false)), state, 50);
        expectEq(run2, List.of("a"), "run2: 'a' reappears at an earlier updatedAt (clock skew) and must be re-synced; 'b' is unchanged");

        // Soft delete: a deleted record inside the window is delivered like any other
        // changed record, not filtered out.
        SyncState delState = new SyncState(0L, new HashMap<>());
        List<String> delRun = Solution.incrementalSync(
            List.of(new Record("x", 10, false), new Record("y", 20, true)), delState, 5);
        expectEq(delRun, List.of("x", "y"), "soft delete: deleted record still appears in the sync set");

        // An unchanged record inside the window is not re-delivered on the next run.
        SyncState stableState = new SyncState(0L, new HashMap<>());
        Solution.incrementalSync(List.of(new Record("p", 100, false)), stableState, 10);
        List<String> stableRun2 = Solution.incrementalSync(List.of(new Record("p", 100, false)), stableState, 10);
        expectEq(stableRun2, List.of(), "stable: an unchanged record inside the window is not re-delivered");

        // Pruning: once an id's updatedAt falls behind the new cursor, it's dropped
        // from recentlySynced -- bounded memory, not an ever-growing map.
        SyncState pruneState = new SyncState(0L, new HashMap<>());
        Solution.incrementalSync(List.of(new Record("old", 100, false)), pruneState, 10);
        // cursor is now 90. A later run whose max is far ahead should prune 'old'
        // (updatedAt 100 <= new cursor) once the cursor moves past it.
        Solution.incrementalSync(List.of(new Record("new", 500, false)), pruneState, 10);
        if (pruneState.recentlySynced.containsKey("old")) {
            System.out.println("FAILED pruning: 'old' should have been pruned once it fell behind the cursor, recentlySynced=" + pruneState.recentlySynced);
            fails++;
        }

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
