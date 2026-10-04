A good answer covers:

- **A connector abstraction with a small, fixed interface.** Something like
  `schema()` (declares tables/columns it produces), `sync(state) -> (records, new_state)`,
  and credential/config handling. The platform core only ever calls this interface -- it
  never knows Salesforce from a SQL database. Strategy pattern, named explicitly or not.
  A good answer separates "what every connector does" (scheduling, retry, checkpoint
  storage, writing to the destination) from "what's unique per source" (auth, pagination
  shape, rate limits), and puts the second part entirely behind the interface.
- **Scheduling that can't double-run.** Names a lock or lease per connector (a row in a
  scheduler table with a `running` flag and a heartbeat, or a distributed lock) so a sync
  that's still running when the next scheduled time arrives doesn't start a second
  overlapping instance against the same source and destination. A vague "we just schedule
  it" without addressing overlap is a gap.
- **Checkpointing for crash-safe resume.** The connector's `sync()` call returns a cursor/
  state object that gets persisted durably (not just in memory) after each batch, so a
  crashed sync resumes from its last checkpoint instead of re-reading the entire source.
  Ties directly to the reconciler/pagination/checkpoint questions elsewhere in this pack --
  a strong answer recognizes this is the same idempotent-sync problem at platform scale.
- **Schema management and drift.** A new table or column showing up in the source gets
  created in the destination automatically; a type change gets handled by a stated policy
  (widen the column type, or alert and pause that table) -- not silently dropped or left to
  crash the load. Mentions that schema changes need to be applied to the destination
  BEFORE the data that needs them lands, not after.
- **Failure isolation between customers/connectors.** Each connector's sync runs as an
  isolated unit of work (a queue-dispatched job, a per-tenant worker pool, or similar) so
  one connector hammering a slow API or hitting a huge backlog doesn't starve other
  customers' syncs. A design where all connectors share one thread pool or one queue with
  no per-tenant fairness is a real gap worth naming.
- **Horizontal scaling story.** Connectors are independent units of work, so scaling out
  is adding more workers that pull from a shared job queue -- no connector-to-worker
  affinity required unless there's a reason (long-lived streaming connections). If the
  candidate proposes sharding by customer or connector ID, a good answer explains how
  rebalancing works when a worker is added or removed (this is where consistent hashing
  is a relevant callback, not required, but a nice connection to make).
- **Handles the mid-round curveball.** When a new requirement gets added (a new connector
  type, a stricter SLA, multi-region), a good answer explains what in the existing design
  absorbs it without a rewrite -- the whole point of the connector abstraction and the
  job-queue-based worker pool is that most new requirements are additive, not structural.

NEEDS_WORK if the answer jumps straight to "we use Kafka and Spark" without ever defining
what a connector actually is or how one syncs incrementally, or if it never addresses what
happens when a sync crashes partway through, or never separates platform-generic logic
from source-specific logic.

## Code

**A connector interface that's too thin -- pushes checkpointing and retry into every
connector implementation instead of the platform:**
```python
class SalesforceConnector:
    def run(self):
        page = 0
        while True:
            data = salesforce_api.fetch(page)   # no checkpoint -- a crash restarts at page 0
            if not data: break
            write_to_destination(data)           # no retry, no backoff
            page += 1
```

**A connector interface that separates platform concerns from source-specific ones:**
```python
class Connector(ABC):
    def schema(self) -> dict: ...
    def sync(self, state: dict) -> tuple[list[Record], dict]: ...
    # auth/config handled by the platform, injected in, not hand-rolled per connector

class SyncRunner:
    def run_once(self, connector, connector_id):
        if not scheduler.try_acquire_lock(connector_id):
            return  # already running, don't double-run
        try:
            state = checkpoint_store.load(connector_id)
            records, new_state = connector.sync(state)
            schema_manager.reconcile(connector.schema())  # DDL before data
            destination.write(records)
            checkpoint_store.save(connector_id, new_state)  # after a durable write
        finally:
            scheduler.release_lock(connector_id)
```
