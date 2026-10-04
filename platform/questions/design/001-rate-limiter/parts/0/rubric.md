A good answer covers:

- **Shared state, not per-node state.** Recognizes that each node counting locally lets a
  client get N requests per node, not N total. Names a shared store (Redis or similar) as
  the fix.
- **Picks an algorithm and defends the pick.** Fixed window is simple but bursts at the
  boundary (2N requests possible right around the edge). Sliding window log or sliding
  window counter fixes that at some memory/CPU cost. Token bucket allows controlled bursts
  on purpose. A good answer names the boundary-burst problem specifically, not just "fixed
  window is less accurate."
- **Concurrency at the shared store.** The increment-and-check has to be atomic (one round
  trip, e.g. `INCR` + `EXPIRE` or a Lua script) or two nodes can both read the same count and
  both allow a request that pushes the client over the limit.
- **Failure mode: the shared store goes down.** Fail open (let requests through, accept
  temporary overuse) or fail closed (reject everything, protect the backend) is a real
  tradeoff to name, not skip.
- **Clock skew** isn't actually a real problem here if the window is measured by the shared
  store's own clock, not each client's or each node's — an answer that raises this and
  resolves it that way is a plus; one that spirals into NTP synchronization is missing the
  point.

NEEDS_WORK if the answer never notices per-node counting undercounts globally, or never
addresses the atomicity of the check when two nodes race.

## Code

**Inefficient/incorrect — per-node counting, and a non-atomic check-then-increment race:**
```python
local_counts = {}   # lives on THIS node only -- N per node, not N total

def allow(client_id):
    count = local_counts.get(client_id, 0)
    if count >= LIMIT:               # two nodes can both pass this check
        return False
    local_counts[client_id] = count + 1   # ...then both increment -- limit exceeded
    return True
```

**Correct — shared store, one atomic round trip:**
```python
def allow(client_id):
    count = redis.incr(f"rate:{client_id}")   # atomic increment, single round trip
    if count == 1:
        redis.expire(f"rate:{client_id}", WINDOW_SECONDS)
    return count <= LIMIT
```

**Alternative — token bucket via a Lua script, allows controlled bursts instead of a hard boundary:**
```lua
-- executed atomically inside Redis; refills tokens based on elapsed time, then checks
local tokens = redis.call('GET', KEYS[1]) or CAPACITY
-- ... refill math, then: if tokens >= 1 then decrement and ALLOW else DENY
```

