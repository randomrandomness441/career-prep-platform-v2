`while (locked_) {}` then `locked_ = true;` is checking, then acting, exactly the pattern
this course keeps coming back to. It needs to become one atomic step: read the current
value AND set it to `true`, together, with the read telling you whether you just won or
someone already had it.

---

`std::atomic<bool>::exchange(true)` does exactly that in one call: it sets the flag to `true`
and returns whatever it was a moment before, atomically. If it returns `true`, someone else
already held the lock (or you're about to spin because you just found that out); if it
returns `false`, you just flipped it from unlocked to locked yourself, and no other thread
can have done that same flip at the same instant.

---

`lock()` is `while (locked_.exchange(true, std::memory_order_acquire)) { }` — keep
exchanging-in `true` until you get back a `false`, meaning it was you who just acquired it.
`unlock()` is a plain `store(false, std::memory_order_release)`. The acquire/release pairing
here does the same job it does for a real mutex: it makes everything the previous lock-holder
did before `unlock()` visible to whoever's `lock()` call picks the flag back up.
