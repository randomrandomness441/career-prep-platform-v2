You're filling in one method of a class named `Solution` (the harness's naming
convention for every submission on this platform -- treat it as the connector class).
The rest of the class, and the driver interface it depends on, are given (defined in
`Tests.java`, same package, no import needed):

```java
interface MockDbDriver {
    // Rows in `table` with updated_at > sinceEpochMillis, ordered by updated_at ascending.
    // Each row is a Map with at least "id" and "updated_at" (a Long, epoch millis).
    List<Map<String, Object>> queryUpdatedSince(String table, long sinceEpochMillis);
}

class Solution {
    private final MockDbDriver driver;
    private long cursor;
    private final List<Map<String, Object>> synced = new ArrayList<>();

    Solution(MockDbDriver driver, long initialCursor) {   // already implemented
        this.driver = driver;
        this.cursor = initialCursor;
    }

    List<Map<String, Object>> getSyncedRows() { return synced; }  // already implemented
    long getCursor() { return cursor; }                           // already implemented

    void syncTable(String table) {
        // TODO: implement this one method.
    }
}
```

A caller just calls `syncTable` on a schedule:

```java
Solution connector = new Solution(driver, 0L);
connector.syncTable("orders");
// ... later, after more rows land in the source ...
connector.syncTable("orders"); // only the new rows get appended this time
```
