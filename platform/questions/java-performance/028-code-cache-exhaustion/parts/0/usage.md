Plain text, three numbered points. Example shape:

```
1. Heap pressure degrades gradually -- more frequent GC, longer pauses.
   Code cache exhaustion is closer to binary: normal right up until it's
   full, then compilation stops entirely for the rest of the process's
   life. There's no gradually-worsening warning period the way heap
   pressure usually gives you.
2. A local test with fewer distinct hot methods or a shorter run can easily
   stay under a reduced cache's ceiling while real production traffic --
   more request types, more code paths going hot, running far longer --
   crosses it. "Worked in testing" isn't the same claim as "will always
   work," since the ceiling depends on workload shape, not a fixed margin.
3. The code cache holds compiled code for distinct methods -- a codebase
   with many different request types or many small classes that all
   genuinely go hot accumulates real cache usage proportional to that
   diversity, independent of total traffic volume. A simpler codebase
   handling the same volume through fewer distinct hot methods never
   approaches the same ceiling even under heavier load.
```
