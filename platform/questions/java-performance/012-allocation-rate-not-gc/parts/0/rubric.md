A good answer covers:

- **The distinguishing signal.** Measure the actual allocation rate (bytes/second, or
  objects/second, visible via an allocation profile like question 008's toolkit, or GC
  logs showing young-gen collection frequency) and compare it against what the workload
  should plausibly need. A high collection *frequency* with normal-sized pauses points at
  allocation rate; long *individual* pauses with normal frequency points more toward
  collector/heap-sizing tuning. A good answer names checking allocation rate specifically,
  not a vague "look at metrics."
- **The hard ceiling on GC tuning.** GC tuning can change *how efficiently* the collector
  processes a given amount of garbage — pause distribution, concurrency, heap layout — but
  it cannot reduce *how much* garbage the application produces. If the app generates 2GB/s
  of garbage, every collector configuration is fighting the same underlying volume; tuning
  can move where that cost shows up (shorter, more frequent pauses vs. longer, rarer ones)
  but there's a floor on total GC work that no flag changes.
- **Why the real fix gets dismissed early.** GC tuning feels like the "expert" move —
  it's specific, technical, and directly addresses the symptom everyone's looking at
  (GC time in a dashboard). Looking at application code for unnecessary allocation feels
  like a step backward, especially once a team is deep in "GC" framing rather than
  "why are we allocating this much" framing — the dashboard metric itself (GC overhead
  %) subtly points attention at the collector even when the collector was never the
  actual problem.

NEEDS_WORK if the answer thinks GC tuning has no ceiling, or can't name a concrete
allocation-rate signal to check before reaching for GC flags.

## Code

**Inefficient — an unnecessary defensive copy on every call, real self-inflicted garbage:**
```java
List<Item> getItems() {
    return new ArrayList<>(items);   // full copy, every call, even for read-only use
}
```

**Correct — return an unmodifiable view instead of copying:**
```java
List<Item> getItems() {
    return Collections.unmodifiableList(items);   // no allocation, no copy
}
```

**Alternative — if callers genuinely need their own mutable copy, make that explicit and opt-in rather than the default:**
```java
List<Item> getItems() { return items; }                    // fast path, shared reference
List<Item> getItemsCopy() { return new ArrayList<>(items); } // opt-in, only when needed
```

