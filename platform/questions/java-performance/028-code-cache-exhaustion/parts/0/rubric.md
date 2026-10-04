A good answer covers:

- **A binary resource, not a gradual one.** Heap memory pressure shows up gradually —
  more frequent GC, longer pauses, a slow degradation curve as available space shrinks.
  Code cache exhaustion is closer to binary: compilation works normally right up until
  the cache is full, at which point it stops entirely, all at once, for the rest of the
  process's life. A good answer names this qualitative difference explicitly — you don't
  get an early warning in the form of gradually worsening performance the way you often
  do with heap pressure; you get normal behavior, then a hard wall.
- **The real risk of "it worked in testing."** A local test with fewer distinct hot
  methods, less code path diversity, or a shorter run than production traffic can easily
  stay under a reduced cache's ceiling while a real production workload — more request
  types exercised, more code paths going hot, running far longer — crosses it. A good
  answer names that this is a workload-shape-dependent ceiling, not a fixed safety
  margin verified once and permanently trustworthy, and that testing "it worked" is not
  the same claim as "it will always work" here.
- **Why method-count diversity specifically matters.** The code cache holds compiled
  machine code for *distinct* methods — a codebase with many different code paths that
  all become genuinely hot (many request types, many small classes each doing real work)
  accumulates real code-cache usage proportional to that diversity, independent of how
  much total *traffic* the service handles. A simpler codebase serving the same total
  request volume through fewer distinct hot methods never approaches the same ceiling,
  even under much heavier load — a good answer names diversity of hot code, not raw
  throughput, as the actual driver of code cache pressure.

NEEDS_WORK if the answer expects code cache exhaustion to produce a gradual slowdown, or
thinks a cache size verified once under test load is permanently safe regardless of
production traffic shape.

## Configuration

**Risky — a reduced cache size "verified" only against a small local test:**
```
java -XX:ReservedCodeCacheSize=8m -jar app.jar   # fine in test, untested against real traffic shape
```

**Correct — size against representative production traffic, then monitor the real metric:**
```
java -XX:ReservedCodeCacheSize=128m -jar app.jar
jcmd <pid> VM.info | grep -A3 "CodeCache"    # watch actual usage under real load, not a guess
```

**Alternative — leave it at the JVM's own ergonomic default unless there's a specific footprint constraint forcing a smaller one:**
```
java -jar app.jar    # ~240MB default on this JDK; only override with real evidence
```

