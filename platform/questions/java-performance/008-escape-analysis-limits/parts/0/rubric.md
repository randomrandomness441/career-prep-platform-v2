A good answer covers:

- **Successfully scalar-replaced = invisible.** If escape analysis and scalar
  replacement work, the object never actually gets allocated on the heap at all — there's
  no allocation event for a profiler to capture, so it would be effectively invisible
  (zero or near-zero weight) in an allocation flame graph, not present-but-small. A good
  answer states this as an absence, not a small presence.
- **The size-bias trap.** The one enormous byte array would look bigger in a size-sorted
  profile, even if the millions of tiny boxed `Integer`s cost far more in aggregate
  (millions of small objects means millions of individual allocation and collection
  events — real GC pressure from sheer *count*, not from any single allocation's size).
  Trusting the size column alone can point you at the rare large allocation while missing
  the real GC-pressure driver hiding in allocation *frequency*. A good answer explicitly
  says to check the count column alongside size, not instead of it.
- **How to actually check.** Real, concrete options: JIT compilation logs with escape
  analysis diagnostics enabled (`-XX:+PrintEscapeAnalysis` / `-XX:+PrintEliminateAllocations`
  on debug JDK builds, or equivalent diagnostic flags), or simply comparing an allocation
  profile before and after a change that should have made an object eligible for scalar
  replacement — if the allocation actually disappears from the profile, it was working;
  if it's still there with real weight, escape analysis didn't cover this case. Either is
  acceptable; what matters is the answer proposes *measuring*, not just reasoning about
  it from the source code.

NEEDS_WORK if the answer expects a correctly-scalar-replaced object to still show up
with meaningful weight in an allocation profile, or trusts allocation size alone without
mentioning count.

## Code

**Defeats escape analysis — the object crosses a non-inlined method boundary:**
```java
double distance(Point a, Point b) {
    Vector d = new Vector(b.x - a.x, b.y - a.y);   // may escape if hypot() isn't inlined
    return hypot(d);                                // real allocation, real GC pressure
}
double hypot(Vector v) { return Math.sqrt(v.dx * v.dx + v.dy * v.dy); }
```

**Correct — keep the short-lived value as plain locals, nothing for the JIT to have to prove non-escaping:**
```java
double distance(Point a, Point b) {
    double dx = b.x - a.x, dy = b.y - a.y;
    return Math.sqrt(dx * dx + dy * dy);            // no object ever exists at all
}
```

**Alternative — if the wrapper type is genuinely needed at call sites, keep the hot method small enough to inline:**
```java
double distance(Point a, Point b) {
    Vector d = new Vector(b.x - a.x, b.y - a.y);
    return hypotInline(d);                          // small, trivially inlined -> EA can see through it
}
private double hypotInline(Vector v) { return Math.sqrt(v.dx * v.dx + v.dy * v.dy); }
```

