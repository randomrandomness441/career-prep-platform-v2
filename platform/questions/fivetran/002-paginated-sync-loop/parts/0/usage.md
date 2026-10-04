Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`, which also defines `PageFetcher`, `Page`, and `Checkpoint`):

```java
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

class Solution {
    static List<String> sync(PageFetcher fetcher, Checkpoint checkpoint, int checkpointEvery) {
        // your implementation
    }
}
```

A caller wires it up like this, running it once, simulating a crash, then resuming with
the same checkpoint:

```java
Checkpoint cp = new InMemoryCheckpoint();  // durable in prod, e.g. a row in the target DB
try {
    Solution.sync(liveFetcher, cp, 500);
} catch (RuntimeException crash) {
    // process restarts here in real life
    List<String> rest = Solution.sync(liveFetcher, cp, 500);
}
```
