A good answer covers:

- **Calibrated advice, not hype.** "Always presize, it's huge" overstates a real,
  measured ~7-9% effect by treating it like the order-of-magnitude findings elsewhere in
  this pack. A more honest version: presizing collections is a real, worthwhile
  optimization for large collections built repeatedly in a hot path, not a universal
  priority, and definitely not something to expect dramatic results from. A good answer
  states the actual measured percentage rather than reaching for "huge" or "massive."
- **Reasoning about `HashMap`'s more expensive resize, checked against the data.**
  `HashMap` resizing does genuinely more work per resize than `ArrayList`'s (rehashing
  and redistributing entries across buckets, not just copying references) — reasoning
  alone might predict a bigger relative improvement from avoiding it. The measured data
  doesn't show a dramatically bigger relative gain for `HashMap` (about 7%, close to
  `ArrayList`'s about 9%) — a good answer notices this and offers a plausible reason
  reasoning-alone would miss: resizing, however expensive per-occurrence, is still a
  small fraction of `HashMap`'s *total* construction cost (which is dominated by
  per-element hashing, bucket lookup, and insertion regardless of resizing), so a more
  expensive resize doesn't automatically translate into a proportionally bigger overall
  percentage win from avoiding it.
- **Where to actually spend the effort.** On the few large, repeatedly-built
  collections in hot paths — presizing a handful of short-lived, small collections
  created once yields negligible real benefit (they resize at most once or twice, if at
  all, and the absolute time involved is tiny), while a large collection rebuilt
  thousands of times in a request path is where the real, measurable 7-9% actually adds
  up to something worth having.

NEEDS_WORK if the answer describes the measured effect as dramatic or "huge," or
recommends presizing effort uniformly across all collections regardless of size or
frequency.

## Code

**Inefficient — default capacity, repeated resize-and-copy as it grows to 100,000 elements:**
```java
List<Integer> build(int[] data) {
    List<Integer> list = new ArrayList<>();   // starts at capacity 10
    for (int x : data) list.add(x);
    return list;
}
```

**Correct — presized once the real size is known up front:**
```java
List<Integer> build(int[] data) {
    List<Integer> list = new ArrayList<>(data.length);   // exact capacity, no resize
    for (int x : data) list.add(x);
    return list;
}
```

**Same idea for HashMap — account for the load factor, not just the entry count:**
```java
Map<Integer, Integer> build(int[] keys) {
    int capacity = (int) (keys.length / 0.75f) + 1;   // avoid a resize before load factor triggers
    Map<Integer, Integer> map = new HashMap<>(capacity);
    for (int k : keys) map.put(k, k);
    return map;
}
```

