import java.util.*;

class Solution {
    private final MockDbDriver driver;
    private long cursor;
    private final List<Map<String, Object>> synced = new ArrayList<>();

    Solution(MockDbDriver driver, long initialCursor) {
        this.driver = driver;
        this.cursor = initialCursor;
    }

    List<Map<String, Object>> getSyncedRows() { return synced; }
    long getCursor() { return cursor; }

    void syncTable(String table) {
        List<Map<String, Object>> rows = driver.queryUpdatedSince(table, cursor);
        for (Map<String, Object> row : rows) {
            synced.add(row);
            long updatedAt = (Long) row.get("updated_at");
            if (updatedAt > cursor) cursor = updatedAt;
        }
    }
}
