## 1. Reframe

"Deep call stacks are slow" is too vague to act on. The real, measured shape is a cliff
at a specific, discoverable depth — most realistic layering sits entirely below it and
is nearly free, and the actual engineering question is whether a given design lives
above or below that line, not whether abstraction in the abstract has a cost.

## 2. What was actually measured, for real

A generated chain of one-line delegating methods, JDK 21, 300 million calls per depth
after full warm-up:

```
depth  1:  277ms       depth 12:  901ms
depth  5:  144ms       depth 14:  878ms
depth  9:  275ms       depth 15:  820ms
depth 10:  903ms       depth 20:  939ms
```

A clean, repeatable cliff between depth 9 and depth 10 — not a gradual slope. Separately,
a trivial recursive method with no local variables, default JVM thread stack size, hit
`StackOverflowError` at a real measured depth of approximately 32,797 calls on this
machine — consistent with `-XX:+PrintFlagsFinal`'s reported 2048 (KB) default thread
stack size.

## 3. The broken version, first

The natural instinct on hearing "there's a real cliff around 9-10 levels of wrapping" is
to treat every layer of architecture with suspicion, counting levels defensively. That
overcorrects: the actual finding is that layering below this threshold is close to free,
which is good news for realistic service/manager/repository-style architectures that
rarely exceed single digits of pure pass-through delegation at any one call site. The
failure mode worth actually worrying about is the rarer case — a design that
accumulates real depth at one call site, or a recursive algorithm walking real data
whose depth isn't bounded by the code at all, but by the data it's given.

## 4. Real-world usage

Real, cited "lasagna code" incidents describe genuine over-engineering costs: one
account describes a project with over a hundred classes built for a simple job, using
nearly every pattern from the Gang of Four book, later cut down to roughly ten classes
with barely any functionality lost — a real, if anecdotal, account of abstraction cost
that was about maintainability and cognitive load as much as raw CPU time. Separately,
Java's [deliberate absence of tail-call optimization](https://www.javacodegeeks.com/2026/08/tail-call-optimization-why-the-jvm-doesnt-do-it.html)
(unlike Scala's `@tailrec`) means recursive tree/document walks in Java specifically —
not just static architectural layering — carry a real, finite depth ceiling that other
JVM languages can sometimes eliminate for genuinely tail-recursive code.

## 5. Performance

All measured, all real, all on this machine: the inlining cliff (277-903ms range across
depths 1-25, cliff at exactly depth 10), and the StackOverflowError depth (~32,797
calls, trivial recursive method, default thread stack size).

## 6. Where this solution fails

The inlining cliff measured here is specific to this JVM's tiered-compilation behavior
and this exact machine — the precise depth where it happens, and even which tier's limit
ends up mattering, can differ across JVM versions, vendors (HotSpot vs. other JVMs), and
JIT flags. The qualitative lesson (a threshold exists, most real code sits below it)
generalizes; the specific number 9 or 10 does not automatically transfer to a different
JVM without measuring there too — the same discipline this entire pack keeps returning
to.

## 7. Interview follow-ups

- Would increasing `-XX:MaxInlineLevel` (or the C1 equivalent) as a JVM flag "fix" deep
  architectural layering for free? In principle it raises the depth at which the cliff
  occurs, but it doesn't eliminate the existence of a cliff, and pushing the JIT to
  inline more aggressively can increase compiled code size and code-cache pressure
  elsewhere — a real tradeoff, not a free win, and not a substitute for noticing that a
  design has accumulated unusual depth at one call site in the first place.
- If a deeply nested JSON document (question 021's topic) gets walked recursively by a
  naive recursive descent parser, does that risk connect to this question's
  `StackOverflowError` finding? Yes directly — a sufficiently deeply nested (attacker-
  controlled or just unusually shaped) JSON document walked with one recursive call per
  nesting level can hit exactly this ceiling, which is why production-grade JSON parsers
  typically impose an explicit maximum nesting depth rather than trusting the input to
  stay shallow.
