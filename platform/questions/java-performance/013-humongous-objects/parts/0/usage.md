Plain text, three numbered points. Example shape:

```
1. Yes, 1.5MB is over the 1MB threshold, so they skip young gen entirely and
   go straight to old-gen humongous regions. Being short-lived doesn't make
   them cheap here -- they miss out on the fast, frequent young-gen
   collection path and get reclaimed through slower old-gen/mixed cycles
   instead, regardless of how briefly they actually lived.
2. Humongous objects need contiguous regions, not just enough total free
   space -- repeated allocation and reclamation leaves gaps a later
   humongous object may not fit into even if total free space is sufficient,
   forcing a compacting collection. Normal small-object churn can use any
   scattered free space and doesn't create this pattern.
3. Increase -XX:G1HeapRegionSize -- since the humongous threshold is defined
   as half the region size, a bigger region size raises the bar, and objects
   that were humongous before may no longer be, with zero code changes.
```
