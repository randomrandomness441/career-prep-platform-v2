A sync writes a batch of rows as `INSERT ... ON CONFLICT DO UPDATE` statements. If row B
has a foreign key pointing at row A and both are new in this batch, A has to be written
before B or the insert violates the constraint. You also can't send the whole table in one
statement, so the batch has to be chunked.

Implement:

```java
static List<List<String>> buildBatches(List<Row> rows, int batchSize)
```

`Row` has `id` and `parentId` (the id of the row it depends on, or `null` if it doesn't
depend on anything in this batch).

Return the row ids split into chunks of at most `batchSize`, flattened back out as
`batches.get(0)` + `batches.get(1)` + ... in write order, such that every row comes after
its `parentId` if that parent is also present among `rows`.

### Requirements

- A `parentId` that isn't among `rows` at all (it already exists in the destination table from an earlier sync) imposes no ordering constraint -- that row is free to go in the first available batch.
- If `rows` contains a real dependency cycle (A depends on B, B depends on A, directly or through a longer chain), throw `IllegalArgumentException`. Don't silently drop the rows involved, don't loop forever, and don't produce a batch that puts a row before a parent it actually depends on.
- Every id in `rows` appears in the output exactly once.
