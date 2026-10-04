A good answer covers:

- **Why a lock doesn't fix it.** There's no shared state being mutated unsafely here —
  each field is only ever written by its own thread, so there's no race to synchronize
  away. The slowdown is a hardware-level cache effect (cache-line invalidation from
  physical proximity in memory), not a correctness problem. A lock would add real
  overhead on top of the false-sharing cost without touching its cause at all.
- **Why a CPU flame graph doesn't show it, and what does.** The flame graph would show
  both threads' increment code as normal, unremarkable, cheap-looking operations — the
  actual cost is stalls waiting on cache-coherency traffic between cores, which doesn't
  attribute to any function call in a stack trace. What would reveal it: hardware
  performance counters (cache-miss rate, specifically cross-core cache invalidation
  events) via a tool that reads CPU counters, not a plain call-stack sampler. A good
  answer names that this needs a different class of tool than everything else in this
  pack, not just a longer capture.
- **`LongAdder` vs `@Contended`.** Related but not identical. `LongAdder` uses striping —
  different threads increment different internal cells so they aren't even writing to
  the same memory location, reducing *true* contention, not just false sharing. But the
  cells themselves still need to avoid false-sharing *each other*, so `LongAdder`'s
  internal cell array is itself padded the same way `@Contended` would do it. A strong
  answer names both: striping solves true contention, internal padding (the same
  technique as `@Contended`) solves false sharing between the stripes.

NEEDS_WORK if the answer proposes a lock as the fix, or treats `LongAdder` and
`@Contended` as solving the exact same problem with no distinction.

## Code

**Inefficient — two unrelated counters likely share a cache line:**
```java
class Counters {
    volatile long readCount;    // thread A increments this
    volatile long writeCount;   // thread B increments this, unrelated to A
}
```

**Correct — `@Contended` pads each field into its own cache line:**
```java
class Counters {
    @jdk.internal.vm.annotation.Contended
    volatile long readCount;
    @jdk.internal.vm.annotation.Contended
    volatile long writeCount;
}
// run with: -XX:-RestrictContended   (needed outside java.base to use @Contended)
```

**Alternative — `LongAdder`, when the goal is a high-throughput counter rather than two independent fields:**
```java
class Counters {
    LongAdder readCount = new LongAdder();
    LongAdder writeCount = new LongAdder();
    // readCount.increment(); ... readCount.sum() to read the total
}
```

