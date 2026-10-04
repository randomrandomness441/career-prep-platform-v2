Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`, which also defines `Row`):

```java
class Row {
    final String id;
    final String parentId; // null if this row doesn't depend on another row in the batch
    Row(String id, String parentId) { this.id = id; this.parentId = parentId; }
}

class Solution {
    static List<List<String>> buildBatches(List<Row> rows, int batchSize) {
        // your implementation
    }
}
```

Example: `rows = [Row("child","parent"), Row("parent", null)]`, `batchSize = 10` gives
`[["parent", "child"]]` -- one batch, `parent` written before `child` even though it
appeared second in the input.
