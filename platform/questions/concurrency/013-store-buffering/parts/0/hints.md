Two `std::atomic<int>` flags, one written by each side: `arriveA()` sets its own flag then
reads the other's; `arriveB()` is the mirror image. That much is the same regardless of
memory order. Run it with the flags using `memory_order_relaxed` for both the write and the
read, many thousands of trials, and count how often both calls return 0. It won't be often —
a handful per twenty thousand — but "won't be often" is exactly the kind of bug that survives
code review and a quick manual test.

---

The instinct is: this is a visibility problem, so use `memory_order_release` on the write and
`memory_order_acquire` on the read — the standard fix for "make sure the other thread sees
my write." Try it, run the same thousands of trials. It's better, but is it actually zero
across many repeated runs? Release/acquire creates a *synchronizes-with* edge between one
specific store and the load that reads *that store's value* — but here, thread A's read
targets B's flag, which A's own write (to A's flag) has no synchronizes-with relationship
with at all. There's no single release-acquire pair connecting the two operations that
matter for the invariant.

---

The invariant needs something stronger: a single, global order that every thread agrees on
for *all* sequentially-consistent operations, not just a pairwise relationship between one
writer and one reader. That's what `memory_order_seq_cst` (the default for `std::atomic` if
you name no order at all) provides, and it's the only one of the six memory orders that does.
With seq_cst, A's store and B's store land in *some* order in that single global timeline —
whichever one is second, the other side's read (also seq_cst) is guaranteed to observe it.
