## ELI5: the helper everyone borrows for a minute

There's a prep cook whose whole job is chopping onions. Every station in the kitchen —
the grill, the soup station, the salad station, the sauce station — grabs her for thirty
seconds at a time, all shift long, whenever they need onions. No single photo ever
catches her chopping for more than thirty seconds at once, because whoever borrowed her
lets her go right after. If you only look for "who's busy for one long, continuous
stretch," you'll never spot her, because she never has one. But add up all those thirty
second borrows across the whole shift, and she's the busiest person in the building.

## What you're actually building (understanding)

Folded-stack data from a request-handling service:

```
main;handle_request;auth;log 5
main;handle_request;db_query;log 40
main;handle_request;render;log 35
main;handle_request;send_response 20
```

`log` appears three times, under three completely different parents. In the rendered
flame graph, that means three separate, physically distant boxes labeled `log` — one
next to `auth`, one next to `db_query`, one next to `render` — none of them individually
looking especially wide.

## Requirements

1. Add up `log`'s total width across all three appearances. How does that compare to
   the widest single box in the whole graph (`db_query`, at 40)?
2. A colleague optimizes `db_query` because its single box is the widest thing they can
   see, and moves on. What did they miss, and why would a glance at the picture alone
   not have caught it?
3. Real flame-graph tooling (like Brendan Gregg's `flamegraph.pl`, or the search feature
   in most flame graph viewers) has a way to search for a function name and highlight
   every box with that name, showing the total percentage across all of them. Why is
   that feature specifically necessary, given what you found in part 1?

## Why this matters

This is the single most common way people misuse a flame graph: optimizing the widest
box they can see with their eyes, while the actual biggest cost center is scattered
across the graph in pieces too small to notice individually.
