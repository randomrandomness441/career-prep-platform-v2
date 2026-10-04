## This is question 009 from the base pack, done again for real on a real JVM

Same bug shape, same lesson, a different language with its own profiling toolchain.
If you already did the C++ version, the algorithmic insight will feel familiar — the
point here is the Java-specific part: a real JVM, `async-profiler` attached to it, and
what JIT inlining does to the picture that's specific to how HotSpot compiles code.

## What you're actually building

```java
static boolean seenBefore(List<Integer> seen, int id) {
    for (int s : seen) if (s == id) return true;
    return false;
}

static List<Integer> dedupe(List<Integer> ids) {
    List<Integer> result = new ArrayList<>();
    for (int id : ids) {
        if (!seenBefore(result, id)) {
            result.add(id);
        }
    }
    return result;
}
```

Same contract as before: remove duplicates, keep first-occurrence order. Correct code.
Also badly quadratic, for the same reason as the C++ version — `seenBefore` scans a list
that keeps growing as `dedupe` fills it.

**Profile it yourself before fixing it.** Install `async-profiler`
(`brew install async-profiler` on macOS, or grab a release from its GitHub for Linux),
and run something like:

```
java -agentpath:$(brew --prefix async-profiler)/lib/libasyncProfiler.dylib=start,event=cpu,collapsed,file=out.collapsed -cp . YourMainClass
```

Point it at a standalone program that calls the naive `dedupe` on a large input (a few
hundred thousand ids is plenty).

## Requirements

1. Fix `dedupe` so it produces the exact same output but scales to large inputs.
2. Submit expects a plain class named `Solution` with a static method matching this
   signature: `static List<Integer> dedupe(List<Integer> ids)`.
3. Submitting runs your version against a large input under a real time budget — the
   naive version above will not pass it no matter how many times you retry.

## Why this matters

Every Java-specific lesson in this pack — safepoint bias, escape analysis, allocation
profiling — was in service of being able to actually do this: point a real profiler at
a real running JVM and read what it says, on your own machine, not from a description
of what someone else saw.
