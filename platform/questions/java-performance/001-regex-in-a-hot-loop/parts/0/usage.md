No code, plain text answer. Example shape:

```
1. It's a constant tax per call, not quadratic -- doesn't grow with how much
   work already happened, so the gap stays a flat ~7x instead of exploding at
   scale like dedupe's O(n^2) bug did.
2. Pattern.compile (and its internal parsing) would show up as an unexpectedly
   wide frame under every .matches() call site -- nothing that looks like "my
   code," which is why it's easy to miss reading the source.
3. Compile the Pattern once (static final), then call pattern.matcher(input)
   per call/thread. Pattern is immutable and thread-safe; Matcher holds
   mutable state, so each thread needs its own Matcher, not a shared one.
```

## Real flame graphs, already generated

`regex-naive-flamegraph.html` and `regex-fixed-flamegraph.html`, next to this file —
real async-profiler captures, open either directly in a browser. In the naive one,
`Pattern.compile` (and everything under it — `Pattern.expr`, `Pattern.sequence`,
`Pattern.atom`...) fills almost the entire width. In the fixed one, that whole branch
is **gone** — `Pattern.compile` doesn't appear anywhere in the profile at all, because
it only ran once, outside the timed loop.

