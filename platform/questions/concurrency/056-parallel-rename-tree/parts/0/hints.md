The recursive shape is the easy part: spawn one thread per subdirectory, recurse inside
each, join before returning. The question is how the count of renamed files gets back up to
the caller. Threading a shared `int&` total through every level and having every thread
increment it directly is the natural way to translate a sequential recursive-accumulator
pattern into a parallel one — try it, and check the boilerplate's result against the tests.

---

`++total` on a plain `int` shared by every thread at every level of the recursion is a
read-modify-write with no exclusion. Under a wide, multi-level tree with many concurrent
threads, that loses real increments — not rarely, reliably.

---

You don't need the counter to be shared at all. Have `parallel_rename_all` return its own
count — the files it renamed directly, plus the sum of whatever each of its child threads'
recursive calls returned. Each thread writes its result into its own dedicated slot (one
entry of a `std::vector<int>`, one slot per subdirectory, never shared between threads) —
concurrent threads never touch the same memory, so there's nothing left to synchronize.
