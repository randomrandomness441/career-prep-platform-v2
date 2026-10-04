`memory_order_relaxed` gives you atomicity and nothing else — it orders *nothing*, not even
the other writes your own thread just did. So ask the pairing question: on the producer
side, two things must become visible to the consumer **as a pair, in order** — the element
write `buf_[h] = v`, and the index publish `head_.store(next)`. Which of the two is the
"payload" and which is the "flag", and what order does the flag's store need so the payload
is guaranteed to land first?

---

The publish is `head_.store(next, ...)` — it needs **release**: everything written before
it in this thread (the element) becomes visible to whoever **acquire**-loads `head_` and
sees the new value. So the consumer's `head_.load(...)` in `pop` needs **acquire**. Now run
the mirror image for the other direction: the consumer reads `buf_[t]` *before* publishing
`tail_.store(...)`, and the producer is about to overwrite `buf_[t]` after loading `tail_`
— so `tail_.store` needs release and the producer's `tail_.load` in the fullness check
needs acquire.

---

Four lines change: `head_.store` → release, `head_.load` in `pop` → acquire,
`tail_.store` → release, `tail_.load` in `push`'s fullness check → acquire. Your *own*
index loads (`head_` in `push`, `tail_` in `pop`) stay **relaxed** — you are the only
writer, nobody can have changed it, paying for ordering there is pure waste. Finally, put
`alignas(64)` on `head_` and `tail_` (and measure it): producer and consumer write those
two variables from different cores, and on one cache line each write invalidates the
other core's copy — false sharing.
