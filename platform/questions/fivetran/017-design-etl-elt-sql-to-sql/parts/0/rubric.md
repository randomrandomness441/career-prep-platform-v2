A good answer covers:

- **ELT over ETL, and says why.** Loads raw data into the destination first, transforms
  there using the destination warehouse's own compute (SQL, dbt-style models) instead of
  a separate transform tier. Names the actual reason: transform logic changes far more
  often than extraction logic, and re-running a transform on already-landed raw data is
  cheap, while re-extracting from the source to fix a transform bug is not. An answer that
  picks ETL should still explain the tradeoff (transform compute is isolated from the
  destination, but every change to transform logic risks re-touching the source).
- **Incremental extraction via a cursor, specifically named.** `WHERE updated_at > cursor`
  or log-based CDC (reading the source's write-ahead log / binlog) instead of a full table
  scan per run. A good answer picks one and defends it: cursor-based is simpler but misses
  hard deletes (a row that's gone doesn't show up in a query anymore, it just silently
  stops appearing) and can't see intermediate states; CDC catches everything including
  deletes but needs access to the source's replication log, which isn't always available
  (managed/SaaS sources).
- **Deletes are addressed explicitly, not glossed over.** A `WHERE updated_at > cursor`
  scan never discovers that a row was deleted, since deleted rows just aren't in the
  result set. Needs either a soft-delete flag in the source, a periodic full reconciliation
  pass (compare source and destination key sets, delete what's missing -- the reconciler
  pattern), or CDC. Not mentioning deletes at all is a real gap.
- **Consistency under concurrent source writes.** Reading from a live production database
  mid-write risks reading a row that's mid-transaction or inconsistent with a related row
  updated in the same transaction. Names a mitigation: reading from a read replica, using
  a snapshot isolation level, or accepting eventual consistency with a stated bound (the
  destination catches up within N minutes) rather than promising exact real-time
  consistency it can't deliver.
- **Load ordering respects foreign keys.** New rows in a parent and child table in the
  same run have to land parent-first, or the destination rejects the child insert (or
  silently violates a constraint if it doesn't enforce one). This is the FK-ordering /
  topological-sort problem from this pack's coding questions, at a schema level instead of
  a row level -- a good answer makes that connection.
- **Schema drift handled with a stated policy, not left implicit.** A new column shows up
  as a new column in the destination (usually safe, additive). A type change needs a
  decision: widen the destination column's type, or flag it and pause loading that table
  until a human resolves it. "We just handle it" with no policy named is a gap.

NEEDS_WORK if the answer never distinguishes ETL from ELT with a stated reason, never
addresses how deletes get detected, or never says anything about what "up to date" bounds
actually mean (treats sync as either perfectly real-time or doesn't address staleness at
all).

## Code

**A naive full-refresh extraction -- correct but doesn't scale, and the "incremental"
requirement is the whole point of the question:**
```sql
-- every run, every table, every row -- rebuilds the destination from scratch
TRUNCATE destination.orders;
INSERT INTO destination.orders SELECT * FROM source.orders;
```

**Incremental extraction with a cursor, applied in dependency order:**
```sql
-- extract only what changed since the last run
SELECT * FROM source.customers WHERE updated_at > :cursor ORDER BY updated_at;
SELECT * FROM source.orders    WHERE updated_at > :cursor ORDER BY updated_at;

-- load customers (the parent) before orders (the child with a FK into customers),
-- inside one transaction per table so a failure doesn't leave a half-applied batch
BEGIN;
INSERT INTO destination.customers (...) VALUES (...)
  ON CONFLICT (id) DO UPDATE SET ...;
COMMIT;

BEGIN;
INSERT INTO destination.orders (...) VALUES (...)
  ON CONFLICT (id) DO UPDATE SET ...;
COMMIT;

-- periodic reconciliation pass catches deletes a cursor-based scan can't see
SELECT id FROM destination.orders
EXCEPT
SELECT id FROM source.orders;
-- -> delete these ids from destination.orders
```
