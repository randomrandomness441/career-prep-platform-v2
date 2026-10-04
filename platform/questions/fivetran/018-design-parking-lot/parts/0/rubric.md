A good answer covers:

- **A schema that models slot types as data, not code.** `slot(id, floor_id, type, status)`
  where `type` is `TWO_WHEELER` or `FOUR_WHEELER` (or a separate `slot_type` table if the
  candidate wants to add types later without a schema migration). A `vehicle(id, type,
  license_plate)` and a `ticket(id, vehicle_id, slot_id, entry_time, exit_time, amount)`
  round out the core. A design that hardcodes "2-wheeler section" and "4-wheeler section"
  as separate, differently-shaped systems instead of a `type` column is a real gap --
  it can't extend to a third vehicle type without a rewrite.
- **Roles map to a clear, separate set of operations.** Owner: configure floors/slots,
  set pricing rules. Security guard: check a vehicle in (issue a ticket, assign a slot),
  check a vehicle out (close the ticket, trigger payment). Person parking: usually doesn't
  operate the system directly in this framing (the guard does it at the gate) -- a good
  answer notices this and doesn't invent a self-service flow that contradicts the given
  roles, or explicitly states the assumption if it adds one (e.g. an automated gate with
  ANPR instead of a guard).
- **Slot assignment picks the smallest sufficient slot, not just "any free one."** A
  4-wheeler needs a 4-wheeler slot; a good answer considers whether a 2-wheeler could ever
  use a 4-wheeler slot as overflow (usually not, since sizes/designations differ) and
  states the policy either way instead of leaving it undefined.
- **The concurrency question is actually answered, not waved at.** Two vehicles, one slot,
  same instant: the naive "find a free slot then assign it" is a check-then-act race --
  two guards' terminals can both see the slot free and both issue a ticket for it. A good
  answer names a fix: a DB-level atomic claim (`UPDATE slots SET status='OCCUPIED' WHERE
  id=? AND status='FREE'` and check the row count, or `SELECT ... FOR UPDATE`), not just
  "we'll use a mutex" without saying what it protects or where it lives in a
  multi-terminal, multi-process system.
- **Fee calculation is a pluggable rule, not a hardcoded formula in the ticket-closing
  code.** Owner configures pricing (flat rate, per-hour, vehicle-type-specific); the
  checkout flow just looks up and applies whatever rule is currently configured. This is
  the same Strategy-pattern instinct the interviewer is checking for, applied to pricing
  instead of connectors.
- **Survives the layered-on requirement.** When the interviewer adds reserved slots,
  multiple gates, or a monthly pass, a good answer shows that the `type`-column,
  `status`-as-state-machine, rule-based-pricing design absorbs it (a reserved slot is a
  `status` value or a reservation table referencing a slot; a monthly pass changes which
  pricing rule applies) rather than needing new tables bolted on everywhere ad hoc.

NEEDS_WORK if the answer never produces an actual schema (table names, columns,
relationships) and stays at a purely verbal level, or if the concurrency race on the last
slot never gets addressed when asked, or if slot types are modeled as separate hardcoded
systems instead of a type/status data model.

## Code

**Check-then-act race on the last slot -- two terminals can both win:**
```sql
-- terminal A and terminal B both run this at nearly the same moment
SELECT id FROM slots WHERE floor_id = ? AND type = 'FOUR_WHEELER' AND status = 'FREE' LIMIT 1;
-- both read slot #42 as free
UPDATE slots SET status = 'OCCUPIED' WHERE id = 42;
-- both succeed -- slot #42 now has two tickets against it
```

**Atomic claim -- the UPDATE itself is the check, one terminal loses cleanly:**
```sql
UPDATE slots
SET status = 'OCCUPIED'
WHERE id = (
  SELECT id FROM slots
  WHERE floor_id = ? AND type = 'FOUR_WHEELER' AND status = 'FREE'
  LIMIT 1 FOR UPDATE SKIP LOCKED
)
RETURNING id;
-- if this returns zero rows, that terminal's guard is told the lot is full for that type
-- and can retry against the next candidate slot instead of racing on the same one
```
