A good answer covers:

- **Not a contradiction — two regimes.** "Delegation is nearly free" and "there's a real
  cliff at a specific depth" are both true, describing different regions of the same
  curve: below the JIT's inlining budget, a chain of wrapper calls gets flattened into
  one unit of compiled code, matching the "nearly free" research finding exactly. Past
  the budget, inlining stops, and every call beyond that point pays a real, un-inlined
  dispatch cost — which is where the measured ~3x jump comes from. A good answer states
  explicitly that both findings are correct, just about different depths, not that one
  contradicts the other.
- **A plausible, honestly-reasoned explanation for tracking C1's limit.** HotSpot compiles
  in tiers: a hot method is compiled quickly by C1 first (with profiling instrumentation),
  and only promoted to C2 after crossing an invocation-count threshold. If the
  C1-compiled version — built with C1's shallower 9-level inline budget — is doing
  meaningful work before or during a benchmark's measured window, the observed behavior
  can reflect C1's limit rather than C2's higher one. A total-inlined-code-size budget
  (separate from the per-level count) being exhausted around the same depth is an
  equally valid, honestly-reasoned alternative. Full credit doesn't require a single
  definitively-proven mechanism — this question doesn't have one confirmed on file
  either — it requires reasoning from the tiered-compilation model rather than just
  restating the observed correlation as if it were self-explanatory.
- **Two independent risks compounding, not one.** No tail-call optimization means a
  recursive tree/document walk pays one real stack frame per level of nesting, with a
  real, finite ceiling (measured here: ~32,800 calls on default settings) — completely
  independent of whether the surrounding architecture's *static* layering is inlined
  away or not. A good answer names this as a *separate* risk from the inlining-cliff
  cost question: a codebase could have perfectly cheap (inlined) static layering and
  still hit `StackOverflowError` from a deeply recursive data structure walked through
  that architecture, because stack depth from recursion and CPU cost from static call
  layering are two different problems that happen to both live in "the call stack."

NEEDS_WORK if the answer treats the two research findings as contradictory, or conflates
the inlining-cost question with the stack-depth-limit question as if they were the same
risk.

## Code

**The inlining risk — a real, unbounded recursive walk through layered code:**
```java
Object visit(JsonNode node, int depth) {        // no depth limit at all
    if (node.isObject()) {
        for (JsonNode child : node) visit(child, depth + 1);   // one frame per level
    }
    return node;
}
```

**Correct — an explicit depth cap turns an unbounded risk into a bounded, controlled failure:**
```java
Object visit(JsonNode node, int depth) {
    if (depth > MAX_NESTING_DEPTH) throw new IllegalArgumentException("nesting too deep");
    if (node.isObject()) {
        for (JsonNode child : node) visit(child, depth + 1);
    }
    return node;
}
```

**Alternative — trade recursion for an explicit stack, removing the call-depth ceiling entirely:**
```java
Deque<JsonNode> work = new ArrayDeque<>();
work.push(root);
while (!work.isEmpty()) {
    JsonNode node = work.pop();
    if (node.isObject()) for (JsonNode child : node) work.push(child);   // heap, not call stack
}
```

