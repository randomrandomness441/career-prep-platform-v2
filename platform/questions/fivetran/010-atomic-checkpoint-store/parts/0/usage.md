Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`, which also defines `Storage` and `Checkpoint`):

```java
class Checkpoint {
    final long cursor;
    final List<String> records;
    Checkpoint(long cursor, List<String> records) { this.cursor = cursor; this.records = records; }
    String serialize() { ... }               // already implemented for you
    static Checkpoint parse(String content) { ... }  // already implemented for you
}

interface Storage {
    void writeFile(String path, String content);
    String readFile(String path); // null if the path doesn't exist
    void renameFile(String from, String to);
}

class Solution {
    static void appendAndCommit(Storage store, String currentPath, String tempPath,
                                 long newCursor, List<String> newRecords) { ... }
    static Checkpoint readCommitted(Storage store, String currentPath) { ... }
}
```

A caller commits a checkpoint after every batch it processes:

```java
Solution.appendAndCommit(store, "checkpoint", "checkpoint.tmp", cursor, batch);
// ... later, on restart ...
Checkpoint resumeFrom = Solution.readCommitted(store, "checkpoint");
```
