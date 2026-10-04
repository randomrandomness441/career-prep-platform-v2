`if (owner_[seat] == -1) { owner_[seat] = user_id; return true; }` reads as "check, then
claim" — and that's exactly two separate steps, with nothing stopping another thread from
running its own check in between yours. Run the boilerplate under ThreadSanitizer (the full,
non-quick verification) rather than trusting that the correctness tests alone passed — a
check-then-act race doesn't reliably produce a *visibly* wrong answer on every run, the way
some races do, but it's a real, unguarded concurrent read/write regardless.

---

`std::atomic<int>::compare_exchange_strong(expected, desired)` collapses the check and the
claim into one atomic step: "if the current value equals `expected` (here, -1, meaning
unowned), replace it with `desired` (the user's id) and report success — all as one
operation nothing else can interleave with." Whichever thread's compare-and-swap succeeds is
the only one that can ever win that seat; every other concurrent attempt sees the value has
already changed and fails cleanly.

---

Each seat's `owner_` entry is independent — reserving seat 5 only ever touches seat 5's
atomic, never seat 12's. That's what "no lock" and "no seat blocks another" both come from
for free once you're using per-seat atomics instead of, say, one mutex guarding the whole
booking system.
