A connector pulls records from a paginated source API. The process can crash mid-sync
(container restart, OOM kill) and gets started again from scratch, so it needs a durable
checkpoint to resume from instead of re-reading the whole source every time.

Implement:

```java
static List<String> sync(PageFetcher fetcher, Checkpoint checkpoint, int checkpointEvery)
```

`PageFetcher.fetchPage(cursor)` returns the next `Page` given a cursor (`null` cursor means
start from the beginning). A `Page` has its `records` (ids, in order) and a `nextCursor`
(`null` means that was the last page).

`Checkpoint` is durable storage for the resume position: `getCursor()` reads the last saved
cursor (`null` if nothing has been saved yet), `save(cursor)` persists a new one.

Return the list of record ids fetched during this call, in the order the source returned
them.

### Requirements

- Call `checkpoint.save(cursor)` after every full page once the number of records fetched since the last checkpoint reaches `checkpointEvery`. Never checkpoint in the middle of a page -- only ever save a cursor that points cleanly to the start of the next unfetched page.
- Start fetching from `checkpoint.getCursor()`, not from the beginning, every time `sync` is called.
- If `fetcher.fetchPage` throws (simulating a crash), let the exception propagate. Don't swallow it and don't fetch further pages after that.
- After a crash, calling `sync` again with the same `checkpoint` must fetch every record that was not yet checkpointed, and must not re-fetch any record that was already checkpointed. Across a crash and a resume, no id may be missing and no id may repeat.
