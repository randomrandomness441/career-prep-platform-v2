A good answer covers:

- **The arithmetic and the caveat.** `db_query` dropped 60 → 15 (-45). `cache_lookup` is
  brand new at 45. Net change across these two frames: roughly a wash in raw sample count
  (-45 + 45 = 0), even though the *shape* of the cost moved. A good answer says explicitly
  that "looks different" isn't automatically "net better," and that total end-to-end
  latency/CPU time (not just which frames moved) is what actually settles whether this
  is a win.
- **When the swap is worth it vs. not.** Worth it: if `cache_lookup` is cheaper in
  wall-clock time per sample than `db_query` was (e.g. an in-memory cache hit is genuinely
  faster per unit of work than a real query, even if it happens to consume a similar
  sample count here), or if it moves cost off a scarce/expensive resource (e.g. off a
  shared database connection pool) onto a cheap one (local memory). Not worth it: if
  `cache_lookup` is now creating memory pressure, adding staleness bugs, or the "45" here
  actually represents *more* wall-clock time than the 45 it replaced, because sample count
  and real duration aren't automatically interchangeable across different kinds of work.
- **The color-equals-conclusion mistake.** Same trap as question 004, applied to a new
  tool: color highlights *a* difference, not necessarily *the* dominant one, and it
  doesn't aggregate same-named frames scattered across different parents any more than a
  regular flame graph does. A single red box being the biggest doesn't rule out a smaller
  regression repeated many times elsewhere summing to more.

NEEDS_WORK if the answer treats the new frame's sample count as directly equivalent in
cost to the old one without qualification, or accepts "biggest red box = the regression"
without the caveat from question 004.

## Code

**Before — every call hits the database directly:**
```cpp
Item lookup(int id) {
    return db_query(id);   // main;handle;db_query -- 60 samples
}
```

**After — a cache in front of it, the change this data is diffing:**
```cpp
Item lookup(int id) {
    if (auto v = cache_lookup(id)) return *v;   // main;handle;cache_lookup -- 45 samples
    Item v = db_query(id);                       // main;handle;db_query -- now 15 samples
    cache_insert(id, v);
    return v;
}
```
The differential graph shows the shift honestly. Whether it's a real win still depends
on whether `cache_lookup` is actually cheaper per call than `db_query` was — not
visible from sample counts alone.

