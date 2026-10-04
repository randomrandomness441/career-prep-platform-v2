Run the boilerplate against the tests before trusting "lock the current cell, then lock the
target cell" — it hangs. Two robots trying to swap into each other's cell at the same moment
is exactly a two-mutex deadlock: robot A holds cell 1, blocked waiting for cell 2; robot B
holds cell 2, blocked waiting for cell 1. Neither can ever proceed.

---

This is the same shape as [[005-scoped-lock]]'s bank-transfer deadlock — two mutexes, two
threads, each acquiring them in the order that happens to be opposite the other's. The fix
there generalizes here directly.

---

`std::lock(a, b)` acquires two (or more) mutexes together, using a deadlock-avoidance
algorithm internally, so it doesn't matter which order two different calls name their
mutexes in — it can never produce the AB/BA deadlock a naive pair of sequential
`lock_guard`s can. Follow it with `std::lock_guard`s constructed with `std::adopt_lock` (to
take ownership of the already-acquired locks without trying to lock them again) so they
still unlock automatically, exception-safely, when the function returns.
