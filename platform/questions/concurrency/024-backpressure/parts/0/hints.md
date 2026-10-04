First, what is the invariant? "The queue holds at most `capacity_` items" — and every
policy is a different answer to *who pays* when a push arrives at a full queue. You need a
second condition variable, `not_full_`, because `not_empty_` cannot tell a blocked producer
"room appeared" from "an item appeared": one shared wakeup makes producers and consumers
wake each other, check the predicate, fail, and sleep again. Now walk each policy: what
exactly does Block wait on, what does DropNewest return, what does DropOldest evict?

---

The mechanics: in `push`, `Block` does
`not_full_.wait(lk, [&]{ return q_.size() < capacity_ || closed_.load(); })` and returns
false if closed; `DropNewest` does `dropped_.fetch_add(1, relaxed); return false;`;
`DropOldest` does `q_.pop_front(); dropped_.fetch_add(1, relaxed);` then falls through to
the push. In `pop`, after `pop_front()` the queue has room — `lk.unlock();
not_full_.notify_one();` (notifying while still holding the lock works but costs the woken
thread one bounce off the mutex). Don't forget `close()`: it must wake *both* queues.

---

Watch the shutdown corners. `Block` + `close()` must not leave a producer spinning forever:
the wait predicate includes `closed_`, and after waking you return false. `pop`'s predicate
is `!q_.empty() || closed_` and it returns `nullopt` only when closed *and* drained — a
consumer must still get the items that were accepted before close. And `try_pop` never
notifies anything, which is correct here only because it does not create room via a
blocked-push path... actually check that: it *does* create room. Decide whether the producer
in Block needs it — the predicate re-check after any wakeup covers you; the notify is an
optimisation, the predicate is the correctness.
