## ELI5: sending one letter per stamp, one at a time

You need to mail 50 invitations. The smart way: write all 50 letters, then make one
trip to the post office and hand over the whole stack. The way this bug actually
happens: you write one letter, walk to the post office, mail it, walk home, write the
next letter, walk back, mail it — fifty round trips for fifty letters that could have
been one trip.

## What you're actually building (understanding)

A Spring Data JPA endpoint returns a list of orders, each with its line items:

```java
List<Order> orders = orderRepository.findAll();       // query #1
for (Order o : orders) {
    List<LineItem> items = o.getLineItems();           // lazy-loaded -- fires on access
    total += items.stream().mapToDouble(LineItem::getPrice).sum();
}
```

`@OneToMany` relationships are lazy by default. `getLineItems()` looks like a plain
getter. It is not — the first time you touch it, Hibernate fires a fresh SQL query for
that one order's line items. For 200 orders, that's 1 query to fetch the orders, then
200 more queries, one per order, fired one at a time inside the loop.

If you captured a trace or flame graph of one slow request handling this endpoint, here
is roughly what the query-timing spans would look like, collapsed to one line each:

```
SELECT * FROM orders                                                    1x,  4ms
SELECT * FROM line_items WHERE order_id = ?                           200x,  3ms each
```

## Requirements

1. Total query count for 200 orders, and total time spent just in the `line_items`
   queries (200 x 3ms each, not overlapping — they're issued one at a time inside the
   loop, waiting for each to return before starting the next).
2. What's the specific visual signature that should make you suspect N+1 the moment you
   see it in a trace or flame graph, before you've even read the query text? (Think
   about what 200 near-identical short spans, back to back, actually look like.)
3. The obvious fix is "eager load the line items in the original query" (a `JOIN FETCH`
   or a batch fetch). Name one real cost that fix introduces if `orders` is large and
   most callers of this endpoint never actually touch `getLineItems()`.

## Why this matters

N+1 doesn't show up as one wide box the way an algorithmic bug does — it shows up as
many identical narrow ones. Recognizing that shape on sight is the actual skill; once
you see it, the fix is rarely in doubt.
