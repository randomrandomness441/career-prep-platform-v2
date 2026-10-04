Get the level-synchronous shape working first: split the current frontier across threads,
each thread looks at its nodes' neighbours, an undiscovered neighbour becomes part of the
next frontier. The two things sequential BFS never has to think about: what happens when two
threads, working on two different current-frontier nodes, both look at the *same* neighbour
at once — and how multiple threads all build "the next frontier" without stepping on each
other while doing it.

---

Run the boilerplate against the tests before trusting a plain `visited[v]` check —
`if (!visited[v]) { visited[v] = true; ... }` is a classic check-then-act: two threads can
both read `visited[v]` as `false` before either one writes it back to `true`. Wrapping the
*next_frontier push* in a mutex doesn't fix this — the race already happened one line
earlier, on the check itself, which is unguarded.

---

`std::atomic<bool>::compare_exchange_strong` turns "check, then set" into one atomic
operation: it reads the current value, and if it matches what you expected (`false`), sets
it to your new value (`true`) and returns `true` — all in one step no other thread can split.
Whichever thread's `compare_exchange_strong` succeeds is the *only* thread that ever adds
that node to the next frontier, no matter how many threads reached it at the same instant.
Have each thread build its own local next-frontier vector instead of appending to one shared
vector directly, and merge the small number of local vectors together sequentially once
every thread is done with the current level — that sidesteps needing a lock around the
frontier itself entirely.
