Plain text, three numbered points. Example shape:

```
1. It bet that hot would keep seeing only Circle, since that's all it
   observed during warm-up. A monomorphic call site can compile a direct,
   inlined call instead of a virtual dispatch -- betting on the observed
   pattern continuing is reasonable because the JIT has no way to know
   future traffic, only what it's actually seen.
2. It stops betting wrong eventually -- after enough repeated deopts for the
   same reason at the same call site, HotSpot compiles a megamorphic version
   using general virtual dispatch instead of a speculative type guess.
   Slower per call than a successful monomorphic inline cache, but stable --
   it never needs to deoptimize again when a third type shows up.
3. A feature flag or canary rollout introducing a second implementation into
   a call site that had only ever seen one concrete type since the service
   started warm -- no code change at the call site itself, just a new type
   flowing through it.
```
