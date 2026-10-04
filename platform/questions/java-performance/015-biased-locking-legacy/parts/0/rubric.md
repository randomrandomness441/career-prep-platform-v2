A good answer covers:

- **Why the freeze is global, not local.** Revocation needs to inspect and possibly
  modify the object's header state safely, and the JVM's safepoint mechanism doesn't
  have a narrower "freeze just these two threads" primitive for this operation — a
  safepoint is a whole-JVM coordination point by design, used for many operations (GC
  among them) that genuinely do need every thread paused. Biased lock revocation reuses
  that same global mechanism because building a cheaper, narrower synchronization
  primitive just for this one case wasn't part of the original design. A good answer
  names that the cost is structural (the safepoint mechanism itself), not specific to
  locking logic.
- **Why to suspect biased locking specifically.** The absence of any correlating GC log
  entry is the key signal — a pause with no GC event backing it, on an older JDK
  (biased locking's active era), with real inter-thread lock handoff happening (a lock
  object initially owned by one thread starting to be touched by others) is the specific
  shape. A good answer contrasts this with question 004 (GC pauses always show in GC
  logs) and question 007 (cgroup throttling correlates with CPU quota metrics, not lock
  activity) — biased locking revocation is the "pause with no other explanation" case
  specifically because it isn't GC and isn't scheduling, it's a locking-subsystem
  safepoint.
- **The real question before chasing it.** What JDK version is actually running in
  production. If it's Java 15+ (deprecated) or especially 21+ (off by default), this
  specific cause is very unlikely or structurally impossible, and time is better spent
  elsewhere — a good answer treats "check the JDK version first" as the actual first
  diagnostic step, not an afterthought, given how version-dependent this entire bug
  category is.

NEEDS_WORK if the answer doesn't check JDK version before pursuing this as a live
hypothesis, or can't explain why the safepoint here has to be global.

## Configuration

Legacy (Java 8-14) territory — the lever is a JVM flag, not application code.

**Left on default on an affected JDK, unaware of the risk:**
```
java -jar app.jar    # biased locking on by default, JDK 8-14
```

**Mitigation on an affected JDK you can't yet upgrade off of — disable it explicitly:**
```
java -XX:-UseBiasedLocking -jar app.jar
```

**Better alternative — upgrade past JDK 15, where it's deprecated (21+, off by default):**
```
# no flag needed on 21+; the whole bug category becomes structurally unlikely
```

