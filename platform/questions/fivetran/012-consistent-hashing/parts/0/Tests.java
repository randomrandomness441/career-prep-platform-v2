import java.util.*;

public class Tests {
    static int fails = 0;

    static void expect(boolean cond, String name) {
        if (!cond) { System.out.println("FAILED: " + name); fails++; }
    }

    public static void main(String[] args) {
        List<String> workers = new ArrayList<>();
        for (int i = 0; i < 10; i++) workers.add("w" + i);
        Solution ring = new Solution(workers, 100);

        int n = 4000;
        List<String> keys = new ArrayList<>();
        for (int i = 0; i < n; i++) keys.add("connector-" + i);

        Map<String, String> before = new HashMap<>();
        for (String k : keys) before.put(k, ring.assign(k));

        ring.removeWorker("w3");

        Map<String, String> after = new HashMap<>();
        for (String k : keys) after.put(k, ring.assign(k));

        int moved = 0;
        int wronglyStable = 0, wronglyStillOnRemoved = 0, unexpectedlyMoved = 0;
        for (String k : keys) {
            String b = before.get(k);
            String a = after.get(k);
            if (!b.equals(a)) {
                moved++;
                if (!b.equals("w3")) unexpectedlyMoved++;
                if (a.equals("w3")) wronglyStillOnRemoved++;
            }
        }
        expect(unexpectedlyMoved == 0,
            "removing a worker only reassigns keys that were ON that worker -- " + unexpectedlyMoved +
            " key(s) that were on a different worker changed anyway");
        expect(wronglyStillOnRemoved == 0, "no key is assigned to a worker that was just removed");
        expect(moved > 0, "at least some keys (the ones that were on w3) do move");

        double fractionMoved = moved / (double) n;
        // Ideal is ~1/10 = 0.10 for a 10-worker ring. A generous upper bound (not close
        // to 1.0) is enough to distinguish real consistent hashing from hash % numWorkers,
        // which would move nearly everything.
        expect(fractionMoved < 0.30,
            "removing 1 of 10 workers should reassign roughly 1/10 of keys, not the whole ring -- moved "
                + String.format("%.1f%%", fractionMoved * 100));

        // Symmetric check: adding a worker back should only pull keys onto its own points,
        // never disturb keys staying with their current worker.
        Map<String, String> beforeAdd = after;
        ring.addWorker("w3");
        int addUnexpected = 0;
        for (String k : keys) {
            String b = beforeAdd.get(k);
            String a = ring.assign(k);
            if (!b.equals(a) && !a.equals("w3")) addUnexpected++;
        }
        expect(addUnexpected == 0,
            "adding a worker back only pulls keys onto its own new points, never moves a key to a third, unrelated worker");

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
