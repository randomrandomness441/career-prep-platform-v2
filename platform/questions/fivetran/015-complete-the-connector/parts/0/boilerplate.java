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
        // TODO: implement this method. It should call driver.queryUpdatedSince(table,
        // cursor), append whatever comes back to `synced`, and advance `cursor` to the
        // max updated_at among the rows just fetched.
    }
}
