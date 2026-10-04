`std::atomic_thread_fence(order)` takes a memory order and applies it *at that point in the
code*, independent of any specific atomic variable. Unlike tagging one `.store()` or
`.load()` call, a fence's effect covers every atomic operation around it on that thread —
which is exactly the "one fence, many operations" property the statement is asking you to
use. Where do you place it — before the store, after it, before the load, after it — for it
to matter to the operations you actually care about ordering?

---

Try `std::atomic_thread_fence(std::memory_order_seq_cst)` placed between the store and the
load, on both sides. Run the same trial count 013 used. Does the invariant hold every time,
or does it behave like 013's release/acquire attempt — mostly right, but not reliably?

---

A `seq_cst` fence participates in the same single global order that a `seq_cst` atomic
operation does — placing one between your relaxed store and your relaxed load gives the
store-then-fence-then-load sequence the same ordering guarantee 013's fully-`seq_cst`
version had, without naming `seq_cst` on the atomics themselves. A weaker fence
(`memory_order_release` or `memory_order_acquire` alone) does not give you this — the same
gap that made release/acquire insufficient for the atomics in 013 applies to release-only or
acquire-only fences here, for the same underlying reason.
