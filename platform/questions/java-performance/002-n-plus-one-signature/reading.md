## 1. Reframe

Not every performance bug looks like one wide box. N+1 looks like many identical narrow
ones — the pattern lives in repetition and count, not in any single span's width.

## 3. The broken version, first

The seductive part of N+1 is that every individual line of code looks completely
reasonable: `orderRepository.findAll()` is normal, `order.getLineItems()` looks like a
plain getter, and the loop is a plain for-each. Nothing here trips a code reviewer.
The cost only exists in the *interaction*: lazy loading means each access is secretly a
network round trip, and a loop turns "secretly a round trip" into "secretly N round
trips, one at a time, never overlapping."

## 4. Interview follow-ups

- Why does N+1 hurt so much more in a request handled over a network to a remote
  database than it would if the data were all in local memory? Each query pays real
  round-trip latency (network + query planning), not just query execution time — 200
  round trips at even 1-2ms of pure network latency each adds real, unavoidable wall
  time that batching collapses into one round trip's worth of latency instead of 200.
- Besides `JOIN FETCH` and eager loading, what's another common fix, and what does it
  trade off? A batch-fetch size hint (fetching line items for a batch of N orders at
  once via a single `WHERE order_id IN (...)` instead of either "1 query" or "N
  queries") — a middle ground that trades some over-fetching for far fewer round trips
  than N, without a single trade rewriting the whole relationship to eager by default.
