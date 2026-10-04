Plain text, three numbered points. Example shape:

```
1. No -- "huge win" overstates a real ~7-9% effect. More accurate: worthwhile
   for large collections built repeatedly in a hot path, not a universal
   priority, and don't expect an order-of-magnitude result from it.
2. Reasoning says HashMap resize should save more relatively, since it does
   real rehashing work, not just copying. The measured data doesn't show a
   much bigger relative gain (~7% vs ArrayList's ~9%) -- likely because
   resizing, however expensive per occurrence, is still a small slice of
   HashMap's total construction cost, which is dominated by per-element
   hashing and bucket insertion regardless of resizing.
3. On the large, repeatedly-built collections in hot paths. Presizing a
   handful of small, one-off collections saves close to nothing in absolute
   terms; a large collection rebuilt thousands of times is where the real
   7-9% actually accumulates into something worth having.
```
