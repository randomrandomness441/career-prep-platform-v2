## The problem

Design a hash map that many threads can read from and write to at the same time, without
one thread's put corrupting another thread's concurrent get, and without every operation
serializing behind a single global lock.

Cover, in your answer:

- The basic structure (buckets, an array of them, some form of chaining or open
  addressing) and where exactly concurrent access becomes unsafe in a naive
  single-threaded version.
- Why a single lock around every operation is correct but bad, and what you'd do instead
  to let unrelated operations run in parallel.
- What happens to in-flight reads and writes while the map is resizing (growing the bucket
  array because load factor got too high) -- this is where most naive designs break.
- What consistency guarantee a `get` actually has while a `put` on a different key is
  happening concurrently -- does it always see the latest value, and does that even matter
  here.
- How you'd verify this is actually correct, beyond "it compiles and looks right" --
  what kind of testing or reasoning would catch a subtle bug here that a code review
  wouldn't.

There's no code to write here. Answer in plain writing, the way you'd talk it through on a
whiteboard.
