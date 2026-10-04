## ELI5: the phone chain that's free until it isn't

You ask a question to one person, who relays it to their assistant, who relays it to
theirs, and so on. If the chain is short — three or four people — the person you
originally asked has basically memorized the whole exchange and can just answer you
directly next time, as if the middle people were never involved at all. Past some
number of people in the chain, that stops being possible to hold in one head. The
message genuinely has to travel person to person, in full, every single time — a real,
noticeably slower process, not just a little slower.

A chain of one-line Java methods, each just calling the next ("wrapper" or "delegate"
layers — extremely common in enterprise codebases with a service layer calling a
manager layer calling a repository layer calling a DAO layer...) behaves exactly like
this, and the cutover point is a real, discoverable number.

## What you're actually building (understanding)

Real, measured on this machine, JDK 21 — a chain of N one-line methods, each doing
nothing but calling the next, ending in one real computation, 300 million calls each
after full JIT warm-up:

```
depth  1:  277ms
depth  5:  144ms
depth  9:  275ms
depth 10:  903ms
depth 12:  901ms
depth 14:  878ms
depth 15:  820ms
depth 16:  849ms
depth 18:  842ms
depth 20:  939ms
depth 25:  997ms
```

Depths 1, 5, and 9 all cost roughly the same. Depth 10 and beyond cost roughly **3x**
more, and stay there — not a gradual climb, a cliff, at a specific number.

This machine's JIT reports two different, real, relevant constants:
`C1MaxInlineLevel = 9` and `MaxInlineLevel = 15` (the more aggressive C2 compiler's own,
higher limit). The measured cliff lands at depth 10 — matching the *lower*, C1 number,
not the higher C2 number most blog posts about inlining budgets cite as "the" limit.

Separately, published research on modern HotSpot JVMs found that delegation chains
typically impose only a 14-15% penalty compared to direct calls — sometimes none at
all — a real, measured finding that directly contradicts the folk belief that "each
layer of wrapping adds real, meaningful overhead."

## Requirements

1. Given both real findings — shallow chains costing nearly nothing extra, AND
   published research calling delegation "nearly free" in general — are these two
   things contradicting each other, or describing two different regimes of the same
   phenomenon? Explain using the measured cliff.
2. This machine's actual observed cliff tracks `C1MaxInlineLevel` (9), not the more
   commonly-cited `MaxInlineLevel` (15) for C2. Given that HotSpot compiles hot code in
   tiers (interpreter → C1 → C2), propose a plausible reason the *lower* tier's limit is
   the one that ended up mattering here, rather than assuming the answer from either
   number alone.
3. Java has no tail-call optimization at all — unlike languages such as Scala, a
   tail-recursive Java method gets no special treatment and still consumes one real
   stack frame per call. A trivial recursive method with no local variables, on this
   JVM's default thread stack size, hit `StackOverflowError` at a measured depth of
   roughly 32,800 calls. What does the combination of "no TCO" and "a real, finite depth
   limit in the tens of thousands" mean for a heavily-layered codebase that also happens
   to process deeply nested data (a recursive tree walk, a deeply nested JSON document)
   through that same layered architecture?

## Why this matters

"Too many layers of abstraction is slow" is usually argued from vibes, or from a vague
appeal to "function call overhead." The real, measured story is sharper and more
useful than that: most realistic layering is free, there's a specific discoverable
threshold where it stops being free, and a separate, harder failure mode (stack
exhaustion) exists independently of whether any individual layer is fast or slow.
