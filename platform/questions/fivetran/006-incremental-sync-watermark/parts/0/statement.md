A connector does incremental sync with `WHERE updated_at > cursor`, saving the max
`updated_at` it has seen as the new cursor after every run. That works until the source's
clock skews: a row gets rewritten with an `updated_at` earlier than one you already synced
(a follower with a slow clock, a batch job stamping rows with its start time instead of now).
Once your cursor has passed that timestamp, a plain `>` comparison can never see that row
again -- it's gone from every future sync.

Implement:

```java
static List<String> incrementalSync(List<Record> allRecords, SyncState state, long graceMillis)
```

`Record` has `id`, `updatedAt` (epoch millis), and `deleted` (a soft-delete flag -- the row
still exists in the source, just marked gone). `SyncState` holds `cursor` (the current
high-watermark) and `recentlySynced` (a `Map<String, Long>` of id -> the `updatedAt` you
last delivered for it, for ids still inside the grace window -- mutate both fields in
place).

Return the ids that need to be synced this run, and leave `state` updated for next time.

### Requirements

- Query window: only consider records with `updatedAt > state.cursor`.
- Within that window, an id is only re-delivered if its current `updatedAt` differs from what `state.recentlySynced` says you delivered last time -- this is what catches clock skew: an id reappearing with an *earlier* `updatedAt` than before still counts as "different" and must be re-synced, not silently treated as already handled.
- Deleted records are not filtered out. A soft-deleted row that falls in the window is synced like any other changed row, so the delete can propagate downstream.
- After processing, set `state.cursor` to `max(updatedAt seen this run, old cursor) - graceMillis` -- holding the cursor back by `graceMillis` is what leaves room for a skewed row to still land inside a future query window instead of falling behind it immediately.
- Prune `state.recentlySynced` of any entry whose `updatedAt` is now `<= state.cursor` -- it's fallen out of the window for good, so there's no reason to keep remembering it.
