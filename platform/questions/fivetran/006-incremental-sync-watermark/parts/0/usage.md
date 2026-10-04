Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`, which also defines `Record` and `SyncState`):

```java
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

class Solution {
    static List<String> incrementalSync(List<Record> allRecords, SyncState state, long graceMillis) {
        // your implementation
    }
}
```

A caller re-runs this against the same durable `SyncState` every sync interval:

```java
SyncState state = new SyncState(0L, new HashMap<>());
long grace = 5_000; // 5 seconds of allowed clock skew

List<String> changed = Solution.incrementalSync(fetchAllFromSource(), state, grace);
// ... write `changed` downstream, then persist `state` (cursor + recentlySynced) ...
```
