Plain text, three numbered points. Example shape:

```
1. db_query dropped 60 -> 15 (-45). cache_lookup is new at 45. Net sample-count
   change across the two is roughly zero -- the cost moved, it didn't
   disappear. Whether that's a win depends on whether cache_lookup work is
   actually cheaper per sample than db_query work was, which the counts alone
   don't say.
2. Worth it if cache_lookup is genuinely faster per unit of work (in-memory hit
   vs a real query) or moves load off a scarce resource like a shared DB
   connection pool. Not worth it if it adds memory pressure or staleness bugs,
   or if its samples actually represent more real time than what they
   replaced.
3. Color just flags a difference, it doesn't sum same-named frames scattered
   under different parents any more than a regular flame graph does. A smaller
   regression repeated many places could add up to more than the single
   biggest red box, same trap as question 4 in a new outfit.
```
