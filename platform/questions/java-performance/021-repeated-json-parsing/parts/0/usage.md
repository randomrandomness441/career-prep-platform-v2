Plain text, three numbered points. Example shape:

```
1. ObjectMapper construction scans the target class via reflection (fields,
   getters, constructors, annotations), builds a serializer/deserializer
   strategy, and sets up several internal caches -- more distinct setup work
   than parsing a single regex string into a state machine, which plausibly
   explains the bigger gap.
2. Parse once, fan out to 10,000 clients: one parse, period. Re-parse per
   client: the one-time cost paid 10,000 times over. The real multiplier is
   client count, and it compounds further with how often the payload updates.
3. When document size or throughput is high enough that the tree model's
   2-14x memory multiplier becomes a real capacity/GC problem -- not by
   default for small or occasional payloads, where the easier-to-write tree
   API is worth its overhead.
```
