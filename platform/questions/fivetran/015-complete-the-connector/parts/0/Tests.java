import java.util.*;

interface MockDbDriver {
    List<Map<String, Object>> queryUpdatedSince(String table, long sinceEpochMillis);
}

class SimpleMockDriver implements MockDbDriver {
    private final Map<String, List<Map<String, Object>>> tables = new HashMap<>();

    void addRow(String table, String id, long updatedAt) {
        Map<String, Object> row = new LinkedHashMap<>();
        row.put("id", id);
        row.put("updated_at", updatedAt);
        tables.computeIfAbsent(table, k -> new ArrayList<>()).add(row);
    }

    public List<Map<String, Object>> queryUpdatedSince(String table, long sinceEpochMillis) {
        List<Map<String, Object>> result = new ArrayList<>();
        for (Map<String, Object> row : tables.getOrDefault(table, List.of())) {
            if ((Long) row.get("updated_at") > sinceEpochMillis) result.add(row);
        }
        result.sort(Comparator.comparingLong(r -> (Long) r.get("updated_at")));
        return result;
    }
}

public class Tests {
    static int fails = 0;

    static void expect(boolean cond, String name) {
        if (!cond) { System.out.println("FAILED: " + name); fails++; }
    }

    public static void main(String[] args) {
        SimpleMockDriver driver = new SimpleMockDriver();
        driver.addRow("orders", "o1", 100);
        driver.addRow("orders", "o2", 200);
        driver.addRow("orders", "o3", 300);

        Solution connector = new Solution(driver, 0L);
        connector.syncTable("orders");
        expect(connector.getSyncedRows().size() == 3, "first sync: fetches every row, got " + connector.getSyncedRows().size());
        expect(connector.getCursor() == 300, "first sync: cursor advances to the max updated_at, got " + connector.getCursor());
        expect("o1".equals(connector.getSyncedRows().get(0).get("id")),
            "first sync: rows land in the order the driver returned them");

        // No new rows -> no-op.
        connector.syncTable("orders");
        expect(connector.getSyncedRows().size() == 3, "no new rows: synced doesn't grow");
        expect(connector.getCursor() == 300, "no new rows: cursor doesn't change");

        // New rows show up -> only the new ones get fetched and appended, cursor advances again.
        driver.addRow("orders", "o4", 400);
        connector.syncTable("orders");
        expect(connector.getSyncedRows().size() == 4, "second sync: only the new row is appended, got " + connector.getSyncedRows().size());
        expect(connector.getCursor() == 400, "second sync: cursor advances to the new max, got " + connector.getCursor());
        expect("o4".equals(connector.getSyncedRows().get(3).get("id")),
            "second sync: earlier rows are untouched, new row appended at the end");

        // A fresh connector starting from a non-zero cursor only picks up rows after it.
        Solution lateStart = new Solution(driver, 200L);
        lateStart.syncTable("orders");
        expect(lateStart.getSyncedRows().size() == 2, "cursor start: only rows after the initial cursor are fetched, got " + lateStart.getSyncedRows().size());
        expect(lateStart.getCursor() == 400, "cursor start: cursor advances to the max among what was fetched");

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
