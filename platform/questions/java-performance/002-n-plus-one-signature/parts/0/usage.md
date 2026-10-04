Plain text, three numbered points. Example shape:

```
1. 201 total queries (1 + 200). 200 x 3ms = 600ms just in sequential line_items
   round trips, on top of the 4ms orders query.
2. A long run of near-identical, narrow, same-shaped spans repeated back to
   back -- you can suspect N+1 from the shape and repetition alone, before
   reading a single query.
3. Eager-loading costs every caller, even the ones who never touch
   getLineItems() -- you trade N+1 for a bigger query paid by everyone,
   whether they needed the data or not.
```
