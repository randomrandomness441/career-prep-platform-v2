Plain text, three numbered points. Example shape:

```
1. Effectively invisible, not small -- if scalar replacement worked, the
   object was never actually allocated on the heap, so there's no allocation
   event for the profiler to capture at all.
2. The one big byte array would look bigger by size, even though millions of
   tiny boxed Integers likely cost more in aggregate GC pressure from sheer
   count. Trusting size alone can point you at the wrong thing -- check the
   count column too.
3. Compare an allocation profile before and after a change meant to make the
   object scalar-replaceable -- if the allocation disappears, it worked. Or
   use JIT diagnostic flags that print escape-analysis decisions directly,
   rather than assuming from reading the source.
```
