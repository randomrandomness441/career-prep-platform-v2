import java.util.*;

class Record {
    final String id;
    final String checksum;
    Record(String id, String checksum) { this.id = id; this.checksum = checksum; }
}

class Delta {
    final List<String> toInsert;
    final List<String> toUpdate;
    final List<String> toDelete;
    Delta(List<String> toInsert, List<String> toUpdate, List<String> toDelete) {
        this.toInsert = toInsert; this.toUpdate = toUpdate; this.toDelete = toDelete;
    }
}

public class Tests {
    static int fails = 0;

    static void expectEq(List<String> got, List<String> want, String name) {
        List<String> g = new ArrayList<>(got); Collections.sort(g);
        List<String> w = new ArrayList<>(want); Collections.sort(w);
        if (!g.equals(w)) {
            System.out.printf("%s: got %s want %s%n", name, g, w);
            fails++;
        }
    }

    public static void main(String[] args) {
        Delta d1 = Solution.reconcile(List.of(), List.of());
        expectEq(d1.toInsert, List.of(), "empty/empty insert");
        expectEq(d1.toUpdate, List.of(), "empty/empty update");
        expectEq(d1.toDelete, List.of(), "empty/empty delete");

        Delta d2 = Solution.reconcile(
            List.of(new Record("a", "h1"), new Record("b", "h2")), List.of());
        expectEq(d2.toInsert, List.of("a", "b"), "pure insert");
        expectEq(d2.toDelete, List.of(), "pure insert -- no deletes");

        Delta d3 = Solution.reconcile(
            List.of(), List.of(new Record("a", "h1"), new Record("b", "h2")));
        expectEq(d3.toDelete, List.of("a", "b"), "pure delete");

        Delta d4 = Solution.reconcile(
            List.of(new Record("a", "h1")), List.of(new Record("a", "h1")));
        expectEq(d4.toUpdate, List.of(), "unchanged record is not an update");
        expectEq(d4.toInsert, List.of(), "unchanged record is not an insert");
        expectEq(d4.toDelete, List.of(), "unchanged record is not a delete");

        Delta d5 = Solution.reconcile(
            List.of(new Record("a", "h2")), List.of(new Record("a", "h1")));
        expectEq(d5.toUpdate, List.of("a"), "changed checksum is an update");

        List<Record> source = List.of(
            new Record("keep", "h1"), new Record("change", "h2new"), new Record("new1", "hX"));
        List<Record> target = List.of(
            new Record("keep", "h1"), new Record("change", "h2old"), new Record("gone", "hY"));
        Delta d6 = Solution.reconcile(source, target);
        expectEq(d6.toInsert, List.of("new1"), "mixed: insert");
        expectEq(d6.toUpdate, List.of("change"), "mixed: update");
        expectEq(d6.toDelete, List.of("gone"), "mixed: delete");

        // Duplicate id on the same side is a real production shape (a paginated source
        // fetch can hand back the same row twice). Only requirement: it lands in exactly
        // one bucket, not a specific one -- last-occurrence-wins is the documented choice,
        // but this test only checks consistency, not which checksum "won".
        List<Record> dupSource = List.of(new Record("dup", "v1"), new Record("dup", "v2"));
        Delta d7 = Solution.reconcile(dupSource, List.of());
        int buckets = (d7.toInsert.contains("dup") ? 1 : 0) + (d7.toUpdate.contains("dup") ? 1 : 0)
                    + (d7.toDelete.contains("dup") ? 1 : 0);
        if (buckets != 1) {
            System.out.printf("duplicate source id 'dup' landed in %d buckets, want exactly 1%n", buckets);
            fails++;
        }

        // Bounded-time check: must not be O(n*m). Deterministic synthetic data, no Random.
        final int N = 20000;
        List<Record> bigSource = new ArrayList<>(N);
        List<Record> bigTarget = new ArrayList<>(N);
        for (int i = 0; i < N; i++) {
            bigSource.add(new Record("id" + i, "v" + (i % 3)));
            if (i % 5 != 0) bigTarget.add(new Record("id" + i, "v0"));
        }
        long t0 = System.nanoTime();
        Delta big = Solution.reconcile(bigSource, bigTarget);
        long ms = (System.nanoTime() - t0) / 1_000_000;
        final long BUDGET_MS = 800;
        if (ms > BUDGET_MS) {
            System.out.printf(
                "reconcile(%d, %d) took %dms, must be under %dms.%n" +
                "A nested loop (source.forEach -> target.contains/indexOf) is O(n*m).%n" +
                "Index one side by id in a HashMap first, then it's O(n+m).%n", N, N, ms, BUDGET_MS);
            fails++;
        } else {
            System.out.printf("20k x 20k reconcile: correct-shaped and %dms (budget %dms)%n", ms, BUDGET_MS);
        }

        if (fails > 0) { System.out.printf("%d check(s) failed%n", fails); System.exit(1); }
        System.out.println("all checks passed");
    }
}
