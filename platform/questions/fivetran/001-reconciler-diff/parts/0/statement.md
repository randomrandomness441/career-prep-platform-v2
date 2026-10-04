Fivetran's sync loop runs this exact operation on every table, every sync: compare what
the source has now against what's already loaded, and emit the minimal set of writes.

Implement:

```java
static Delta reconcile(List<Record> source, List<Record> target)
```

`Record` has an `id` (primary key) and a `checksum` (a hash standing in for the row's full
content -- real pipelines diff by hash instead of shipping full rows on every comparison).

Return a `Delta` with three id lists:

- `toInsert` -- ids present in `source` but not in `target`.
- `toUpdate` -- ids present in both, but the checksum differs.
- `toDelete` -- ids present in `target` but not in `source`.

An id present in both with the same checksum appears in none of the three lists.

### Constraints

- Both `source` and `target` can be large (tens of thousands of rows). The intended solution is O(n + m). A nested scan (for each source row, linear-search target) is not fast enough and will fail the timing check.
- Either list may contain a duplicate id -- a paginated source fetch can hand back the same row twice across pages. Treat the last occurrence in the list as authoritative.

`Record` and `Delta` are already defined for you in the test harness (same package, no
import needed) -- see Usage for their exact shape.
