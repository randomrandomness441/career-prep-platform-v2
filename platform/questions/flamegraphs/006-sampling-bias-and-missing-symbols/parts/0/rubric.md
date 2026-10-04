A good answer covers:

- **Fixing the missed short event.** Increase the sampling frequency (e.g. `-F 999` or
  higher for a short, targeted capture) so the interval between samples shrinks closer
  to the event's own duration, raising the odds any single occurrence gets caught. A
  good answer also accepts "use a tracing/instrumentation approach instead of sampling
  for this specific known-suspect function" as a valid alternative, since sampling has
  a hard floor on what it can reliably catch no matter how high the rate goes.
- **The `[unknown]` box.** A real, concrete cause: missing debug symbols (a stripped
  binary or shared library, or a container image built without `-g`/without keeping the
  `.debug` files). A concrete next step: rebuild or re-run with symbols available
  (`objcopy --add-gnu-debuglink`, or ensuring the CI pipeline retains debug info) rather
  than assuming the box represents nothing worth investigating. NEEDS_WORK if the answer
  treats `[unknown]` as "nothing happened there" instead of "we can't see what happened
  there."
- **Skepticism about the flat 40% box.** The alternative explanation: inlining. The
  compiler or JIT may have folded several logically distinct functions into
  `handle_request`'s single native frame, so the profiler has nothing to break down even
  though real internal structure exists in the source. A good answer suggests a concrete
  way to check (disable aggressive inlining for a diagnostic build, or use compiler flags
  that preserve inline-frame information, e.g. debug-non-safepoint options on a JVM)
  rather than accepting the flat box as proof the function is monolithic.

NEEDS_WORK if any of the three answers takes the flame graph's surface reading at face
value without considering what could make the underlying data misleading.

## Code

**Insufficient — low-rate capture, misses a short lock acquisition entirely:**
```
perf record -F 99 -a -g -- ./service
```

**Correct — raise the rate for a short, targeted capture:**
```
perf record -F 999 -a -g -- sleep 5
```

**Fixing missing symbols — a stripped binary produces `[unknown]`, rebuild with debug info kept:**
```
# wrong: strips everything
gcc -O2 -o service service.c

# correct: keep symbols available to the profiler
gcc -O2 -g -o service service.c
objcopy --only-keep-debug service service.debug
objcopy --add-gnu-debuglink=service.debug service
```

