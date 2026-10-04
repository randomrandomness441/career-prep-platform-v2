A good answer covers:

- **What gets misjudged.** A safepoint-biased profiler would under-report the tight,
  safepoint-free loop's real cost and over-report the I/O-heavy method's cost relative
  to it, even if the tight loop is actually far more expensive in real CPU time. The
  profile's relative ranking of "which method costs more" can be inverted from reality,
  not just slightly off.
- **Why safepoints everywhere isn't free.** A safepoint requires every thread to reach
  a checkpoint before any of them can proceed past it — more safepoint checks means more
  frequent brief coordination overhead across every thread in the JVM, all the time, not
  just during profiling. It's a real, constant tax on every run of the program to fix a
  problem that only matters while someone happens to be profiling.
- **A real reason to still use JFR.** JFR is built into the JDK, has very low overhead,
  is safe to leave running continuously in production, and its data format is
  well-supported by tooling (JDK Mission Control). `async-profiler` needs a separate
  native agent installed and attached, and depends on an unsupported internal API
  (`AsyncGetCallTrace` is not a public, guaranteed-stable interface) — a team valuing
  operational simplicity and stability over sampling accuracy might reasonably still
  default to JFR, accepting the safepoint bias as a known limitation.

NEEDS_WORK if the answer claims safepoint bias makes JFR useless, or can't explain why
adding more safepoints has a real cost.

## Configuration

Not an application-code fix — the choice is which profiler to attach.

**Reaching for JFR when a tight, safepoint-poor loop is the actual suspect:**
```
jcmd <pid> JFR.start duration=30s filename=profile.jfr
# risks under-reporting a hot, safepoint-free loop relative to I/O-heavy code
```

**Correct for that specific case — async-profiler, which bypasses safepoint bias:**
```
asprof -d 30 -e cpu -f profile.collapsed <pid>
```

**Alternative — stick with JFR deliberately, when operational simplicity matters more than sampling accuracy:**
```
# Low overhead, no extra agent to install/maintain, safe to run continuously in prod
jcmd <pid> JFR.start settings=profile filename=continuous.jfr
```

