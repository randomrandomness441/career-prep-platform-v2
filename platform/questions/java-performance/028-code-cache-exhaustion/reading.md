## 1. Reframe

The code cache is a fixed-size resource with a hard ceiling, unlike heap memory's
gradual pressure curve. Running out of it doesn't slow the JVM down — it turns off one
of the JVM's core performance mechanisms entirely, for good, for that process.

## 2. What was actually measured, for real

JDK 21, this machine, an intentionally tiny `-XX:ReservedCodeCacheSize=3m` against a
1,500-distinct-method program:

```
[2.803s][warning][codecache] CodeCache is full. Compiler has been disabled.
CodeCache: size=3072Kb used=3047Kb max_used=3068Kb free=24Kb
total_blobs=2182, nmethods=1885, adapters=206, full_count=1
Exception in thread "main" java.lang.VirtualMachineError: Out of space in CodeCache for adapters
```

At the extreme, this doesn't degrade — it crashes. At a slightly larger 4MB cache
(versus this JVM's default of roughly 240MB), the identical program ran to completion
with no measurable timing difference from the default. The gap between "crashes" and
"no measurable effect" was a difference of about 1MB in cache size for this specific
workload.

## 3. The broken version, first

Reducing `-XX:ReservedCodeCacheSize` to save memory footprint (a real, sometimes
legitimate goal in memory-constrained environments) without measuring against
representative production traffic is a real, documented way to introduce a ceiling that
a local test never gets close to, but production eventually does — especially as a
codebase grows more request types or more distinct hot code paths over time, even
without a deliberate code cache change ever being made.

## 4. Interview follow-ups

- Does JIT tiered compilation make code cache pressure worse or better than a
  single-tier compiler would? Worse, in terms of raw cache usage — tiered compilation
  (interpreter → C1 → C2) can mean a single method occupies cache space for *multiple*
  compiled versions of itself over its lifetime (an earlier, less-optimized C1 version
  and a later C2 version) before the earlier one is reclaimed, compared to a JVM that
  only ever compiles a method once.
- Is there a way to see how close a running JVM is to this ceiling before it becomes a
  crisis? Yes — JVM monitoring exposes code cache usage as a real, queryable metric
  (via JMX, or tools built on it), and it's a real, standard thing to alert on in
  production for exactly this reason: catching "approaching the ceiling" while there's
  still time to act, rather than discovering it via a crash.
