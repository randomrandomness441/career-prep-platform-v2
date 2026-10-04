A good answer covers:

- **An explicit state machine, named states and named transitions.** At minimum
  `AVAILABLE -> HELD -> CONFIRMED` and `HELD -> EXPIRED` / `HELD -> CANCELLED`, with
  `CONFIRMED -> CANCELLED` as a separate later transition (a refund flow, different from
  an expired hold). A good answer draws or lists this explicitly rather than describing
  booking as one vague "book it" action. Bonus for noting invalid transitions should be
  rejected at the data layer (a `CONFIRMED` booking can't go back to `HELD`).
- **The hold expires without a constantly-running sweeper.** Two real mechanisms: a TTL on
  the hold row with an index on `expires_at`, checked lazily (whenever anything reads that
  inventory unit's state, check if the hold's TTL passed and if so treat it as expired
  before proceeding) combined with a periodic low-frequency cleanup job as a backstop --
  not a design that depends on a background process running every second to keep the
  system correct. A message-queue delayed-message approach (schedule a "release this hold"
  message for TTL-seconds later) is also a valid answer if the candidate names it.
- **The concurrency fix on the last unit is atomic, not a race.** The hold on a unit is a
  single atomic operation at the database (`UPDATE inventory SET status='HELD', held_by=?
  WHERE id=? AND status='AVAILABLE'`, checking the row count) or via a unique constraint
  that only one `HELD`/`CONFIRMED` row can reference a given inventory unit at a time. A
  design that reads "is it available?" and then writes "hold it" as two separate steps,
  even with a comment saying "in a transaction," needs the isolation level or locking
  mechanism named specifically -- "we use a transaction" alone doesn't rule out a race
  under the default isolation level most databases actually ship with.
- **Payment is decoupled from the hold, not synchronous with it.** The hold happens first
  (fast, holds the inventory), then payment processes separately (can be slow, can fail,
  can involve a third-party gateway and webhook callbacks) and its result drives the
  `HELD -> CONFIRMED` or `HELD -> CANCELLED` transition. A design that tries to hold
  inventory AND charge the card in one synchronous step either holds the row lock open too
  long or has no story for "payment succeeded but the confirmation write failed."
- **Extends to seat selection without a redesign.** The core insight: seat selection means
  Inventory becomes more granular (a specific seat, not a pool of N identical units) --
  the SAME state machine, the SAME hold/confirm/expire lifecycle, applies to each seat as
  its own Inventory row. A good answer recognizes this is a data-modeling change (finer
  grained Inventory rows) more than a new subsystem. A design that treats "seat selection"
  as requiring a completely separate booking flow is missing the reuse.

NEEDS_WORK if the state machine is never made explicit (booking is described as a single
step), if the concurrency question gets a hand-wavy "we use a transaction" with no
isolation level, locking mechanism, or atomic operation actually named, or if the hold
expiry story requires a constantly-polling background sweeper with no lazy-check fallback.

## Code

**Check-then-act hold -- a classic race on the last unit:**
```sql
-- two users' requests interleave on the very last available room
SELECT status FROM inventory WHERE id = 42;  -- both read 'AVAILABLE'
UPDATE inventory SET status = 'HELD', held_by = ? WHERE id = 42;
-- both updates succeed under a naive read-then-write -- two holds on one room
```

**Atomic hold -- the UPDATE's WHERE clause is the check:**
```sql
UPDATE inventory
SET status = 'HELD', held_by = ?, expires_at = now() + interval '10 minutes'
WHERE id = 42 AND status = 'AVAILABLE';
-- if this affects 0 rows, that request lost the race and is told the room is gone
```

**Lazy expiry check, no dedicated sweeper needed for correctness:**
```sql
-- any read of this row treats an unexpired-looking hold as actually expired if its
-- TTL has passed, before deciding what to do with it
SELECT *, (status = 'HELD' AND expires_at < now()) AS effectively_expired
FROM inventory WHERE id = 42;
-- a periodic low-frequency job still reclaims these for cleanliness, but correctness
-- never depends on it running promptly
```
