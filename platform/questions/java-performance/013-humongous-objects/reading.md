## 1. Reframe

"Short-lived objects are cheap" is a claim about the *young generation's* design, not
about object lifetime in the abstract. Cross the humongous size threshold and an
object stops benefiting from that design entirely, no matter how short its real
lifetime is.

## 3. The broken version, first

A team notices GC overhead climbing after adding a feature that builds moderately large
in-memory buffers per request, assumes it's a normal allocation-rate problem (question
012's lesson), and starts looking at request volume and object count. The real driver
is size, not count or rate — a much smaller number of allocations than a typical
allocation-rate problem, each one individually crossing the humongous threshold and
paying a fragmentation cost that scales differently than ordinary garbage volume does.
Without checking the specific size of what's being allocated against the region size,
this looks like a generic "too much garbage" problem instead of the size-triggered one
it actually is.

## 4. Interview follow-ups

- Would switching to ZGC or Shenandoah (question 004's collectors) make the humongous
  object problem disappear? Not automatically — the general principle that
  unusually-large objects cost more to place and reclaim than the collector's
  normal-sized-object fast path exists in some form across generational and
  region-based collectors; ZGC and Shenandoah have their own large-object handling with
  different specifics, but "very large objects aren't free just because they're
  short-lived" isn't a G1-only phenomenon.
- If a service can't avoid genuinely needing objects near or above the region size
  (e.g., large image buffers), is there an application-level fix besides growing the
  region size? Reusing buffers via a pool instead of allocating fresh ones per request —
  turning "allocate and immediately discard a humongous object" into "check out and
  return a long-lived one," which sidesteps the humongous-allocation-and-reclaim cycle
  entirely rather than trying to make it cheaper.
