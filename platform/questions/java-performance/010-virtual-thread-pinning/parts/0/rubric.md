A good answer covers:

- **The actual constrained resource.** It's not virtual-thread count (that's cheap and
  effectively unlimited) and it's not the request queue — it's the small, fixed pool of
  *carrier* threads. Counting virtual threads or queued requests tells you nothing about
  how many carriers are currently pinned, which is the only number that actually matters
  here. A good answer explicitly separates "how many virtual threads exist" from "how
  many carriers are available to run any of them right now," and names that the second
  number can hit zero while the first number is in the thousands.
- **A real detection mechanism.** JFR emits a `jdk.VirtualThreadPinned` event whenever a
  virtual thread pins, since JDK 21 — enabling this event in a profile or continuous
  recording surfaces exactly when and where pinning happens. `-Djdk.tracePinnedThreads=full`
  (a JVM diagnostic flag) is another real, concrete option, printing a stack trace at the
  pin site. Either is acceptable; "add logging/print statements" is not a real answer to
  what's being asked.
- **The tradeoff between the two fixes.** `ReentrantLock` fixes it immediately, on the
  current JDK, but means finding and changing every `synchronized` block on a blocking
  path across the codebase — real, distributed engineering effort, easy to miss one.
  Upgrading to JDK 24+ fixes every instance at once with no code changes, but means
  waiting on (and being ready for) a JDK upgrade, which carries its own real migration
  risk and timeline that a team may not control on demand. A good answer names both as
  real costs, not "just do the obviously correct one."

NEEDS_WORK if the answer thinks virtual-thread count or queue depth would reveal this
directly, or can't name a real detection mechanism beyond ad hoc logging.

## Code

**Inefficient on JDK 21-23 — synchronized pins the carrier for the whole blocking call:**
```java
synchronized (lock) {
    String result = slowHttpCall();   // virtual thread can't unmount while holding this
}
```

**Correct — ReentrantLock doesn't pin, the carrier is free while blocked:**
```java
private final ReentrantLock lock = new ReentrantLock();

lock.lock();
try {
    String result = slowHttpCall();   // unmounts normally, carrier serves other work
} finally {
    lock.unlock();
}
```

**Alternative — no code change, upgrade the JDK (fixes every `synchronized` site at once):**
```
# JDK 24+ (JEP 491): synchronized no longer pins in the common case
```

