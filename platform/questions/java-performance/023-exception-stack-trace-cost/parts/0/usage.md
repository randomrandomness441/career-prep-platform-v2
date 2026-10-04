Plain text, three numbered points. Example shape:

```
1. fillInStackTrace() walks the entire call stack at construction time,
   recording every frame -- real work proportional to stack depth, done
   unconditionally before any throw/catch happens. It fires on "new
   SomeException()" alone, even if never thrown.
2. Reasonable for a tightly-scoped internal signal that never crosses a
   boundary a human or system needs to diagnose. Dangerous at a public API
   boundary or anywhere it might reach a log or a caller -- you'd be
   trading away the only information that lets anyone debug an unexpected
   failure.
3. Once per instance. The cost is tied to construction (fillInStackTrace
   runs in Throwable's constructor), not to the throw keyword -- reusing one
   pre-built instance and throwing it repeatedly pays the capture cost
   exactly once.
```

## Real flame graphs, already generated

`exception-naive-flamegraph.html` and `exception-fixed-flamegraph.html`, next to this
file — real captures. In the naive one, the entire width above `doWork` is
`fillInStackTrace` → `BacktraceBuilder` → native stack-walking machinery — nothing
else is happening. In the fixed one, that whole branch disappears completely: the
work became so cheap that all that's left in the profile is the benchmark loop's own
`System.nanoTime()` timing checks — `doWork` and `Signal` don't show up as separate
frames at all anymore.

