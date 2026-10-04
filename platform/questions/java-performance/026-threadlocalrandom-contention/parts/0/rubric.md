A good answer covers:

- **Why single-threaded testing hides it.** Contention requires at least two threads
  actually competing for the same mutable seed at the same time — a single thread never
  fails a CAS attempt because nothing else is racing it. A good answer states plainly
  that this cost is fundamentally about concurrent access, not about `Random` itself
  being slow, and that any test run single-threaded (a common default for a quick
  correctness check) will never reveal it no matter how many iterations it runs.
- **Why CAS contention can be worse than a lock.** A thread blocked on a held lock
  parks — it stops consuming CPU and waits to be woken. A thread whose CAS attempt fails
  immediately retries: reads the current value again, recomputes, attempts again — all
  of that retry work is real CPU spent accomplishing nothing, and more competing threads
  means more failed attempts, means more wasted retries, means the *next* attempt is
  more likely to fail too. A good answer names the difference: a lock's cost is mostly
  waiting; failed CAS's cost is mostly wasted, repeated work, and that gap widens as
  contention increases rather than staying flat.
- **Independent seeds by design, and the real drop-in caveat.** `ThreadLocalRandom`
  deliberately seeds each thread independently, specifically to avoid threads producing
  correlated sequences — no, two threads won't produce the same sequence. The real
  reason it isn't automatically a safe drop-in for *every* use of shared `Random`: it
  doesn't support setting an explicit seed the way `new Random(seed)` does, so code that
  depends on a fixed, shared seed for reproducible test output loses that reproducibility
  by switching. A good answer names this specific tradeoff rather than a vague "it's
  less predictable" concern.

NEEDS_WORK if the answer thinks the contention cost would show up in a single-threaded
test, or claims CAS contention behaves the same as lock contention under load.

## Code

**Inefficient — one shared Random, every thread fights over its seed:**
```java
private final Random shared = new Random(42);   // one instance, one field

int pick() {
    return shared.nextInt(1000);   // every thread CASes the same seed
}
```

**Correct — each thread gets its own independent generator:**
```java
int pick() {
    return ThreadLocalRandom.current().nextInt(1000);   // no shared state, no CAS race
}
```

**Alternative — if reproducibility across runs is required, seed per-thread explicitly instead of sharing one instance:**
```java
private final ThreadLocal<Random> perThread =
    ThreadLocal.withInitial(() -> new Random(Thread.currentThread().getId()));

int pick() {
    return perThread.get().nextInt(1000);   // independent, still reproducible per thread
}
```

