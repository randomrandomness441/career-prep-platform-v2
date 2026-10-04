Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`, which also defines `Record` and `Delta`):

```java
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

class Solution {
    static Delta reconcile(List<Record> source, List<Record> target) {
        // your implementation
    }
}
```

Example: `source = [("a","h1"), ("b","h2new")]`, `target = [("b","h2old"), ("c","h3")]`
gives `toInsert=["a"]`, `toUpdate=["b"]`, `toDelete=["c"]`.
