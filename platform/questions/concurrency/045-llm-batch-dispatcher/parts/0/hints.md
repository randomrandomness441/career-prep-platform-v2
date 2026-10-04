Run the boilerplate against the full (non-quick) verification, not just the correctness
tests — it may well pass the correctness stage and still be wrong. `pending_` is a plain
`std::vector`, and `submit()` is called from many threads at once with nothing guarding
`push_back`/`emplace_back` on it at all. Growing a `std::vector` can reallocate its whole
backing buffer; two threads racing inside that operation at the same moment is a genuine
data race on ordinary (non-atomic) memory, which ThreadSanitizer will catch even on a run
where the correctness check happens to still pass.

---

`pending_` only needs protecting while it's actually being mutated — a `std::mutex` around
the body of `submit()` (and around `flush()`'s handoff of the pending list) is enough; there's
no need for anything fancier like per-request atomics, since the whole point of `flush()` is
to grab the entire batch at once anyway.

---

`std::promise`/`std::future` is what lets you hand a request-specific answer back to a
specific caller without them ever needing to know a batch was involved:
[[007-async-future]]'s `packaged_task`-based `spawn_task` is a close relative — here, though,
you construct the `promise` yourself in `submit()`, store it alongside its request, and only
call `set_value()` on it later, once `flush()` actually has that request's response in hand.
