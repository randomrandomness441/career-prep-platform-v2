The buffer of records itself can be sized once, at construction — a `std::vector<LogRecord>`
allocated once in the constructor and never resized after that satisfies "log() never
allocates," since `log()` never touches the vector's own allocation, only writes into
elements that already exist. The real question is: when two producer threads call `log()` at
the same moment, how does each one get a *different* slot to write into?

---

Run the boilerplate against the tests before assuming it's close enough — it loses hundreds
of messages out of every batch, every run. `next_write_` there is a plain `std::size_t`,
incremented with `next_write_++`. Under concurrent calls, that's a read-modify-write with no
exclusion: two threads can both read the same value before either writes back the
incremented result, and both get handed the *same* slot index.

---

`std::atomic<std::size_t>::fetch_add` solves the slot-assignment problem in one call: it
atomically reads the current value, adds one, and returns the value *before* the add — so
two concurrent callers are guaranteed to receive two different indices, no matter how they
interleave. Once each producer owns a distinct slot, writing that producer's fields into it
needs no further coordination with any other producer — they're touching disjoint memory.
What still needs care is telling the consumer a slot is *fully* written before it reads it —
a plain write your producer does last isn't enough on its own; think about what ordering
guarantee the consumer's read needs relative to your writes, the same tool you'd reach for
in [[016-spsc-ring-buffer]].
