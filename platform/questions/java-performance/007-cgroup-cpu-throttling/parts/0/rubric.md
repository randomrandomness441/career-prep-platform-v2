A good answer covers:

- **Why GC pauses specifically expose this.** Steady-state request handling rarely
  spawns many threads trying to run simultaneously. A stop-the-world GC pause is exactly
  the moment a JVM tries to run many parallel GC worker threads at once, all racing to
  finish before the application can resume — that burst of simultaneous demand is what
  collides hardest with a tight CPU quota, burning through an entire scheduling period's
  allowance almost immediately and triggering throttling right when the application can
  least afford to lose more time.
- **What to check beyond `UseContainerSupport` being on.** Whether the CPU limit is
  fractional (2.5 vCPUs, likely rounding oddly) or effectively unset at the container
  level despite looking set elsewhere, and whether `-XX:ActiveProcessorCount` should be
  set explicitly rather than trusting auto-detection — `UseContainerSupport` being
  enabled is necessary but the searches above show it isn't sufficient on its own for
  every configuration; the actual detected processor count is worth verifying directly
  rather than assumed correct.
- **What a CPU flame graph would show: a gap, not high usage.** CPU throttling means the
  kernel's CFS scheduler stops the process from running at all for the rest of that
  scheduling period once the quota is exhausted — throttled time is off-CPU time from
  the profiler's point of view, structurally identical in shape to the GC-pause blind
  spot from question 004. A CPU flame graph over the whole spike window would show
  *low or gapped* usage, not high usage, because a real chunk of that window the process
  was forcibly not running at all, even though it briefly hit 100%+ of its quota right
  before being cut off.

NEEDS_WORK if the answer expects a CPU flame graph to show sustained high usage during
throttling, or treats `UseContainerSupport` as a complete fix on its own.

## Configuration

**Insufficient — relying on auto-detection alone with a fractional CPU limit:**
```yaml
resources:
  limits:
    cpu: "2.5"       # UseContainerSupport is on by default, but rounding here is risky
```

**Correct — verify and, if needed, pin the detected processor count explicitly:**
```
java -XX:+PrintFlagsFinal -version | grep ActiveProcessorCount
# if it's not what the container quota actually grants:
java -XX:ActiveProcessorCount=2 -jar app.jar
```

**Alternative — round the container limit up to a whole number instead of overriding the JVM:**
```yaml
resources:
  limits:
    cpu: "3"          # avoids the fractional-rounding ambiguity at the source
```

