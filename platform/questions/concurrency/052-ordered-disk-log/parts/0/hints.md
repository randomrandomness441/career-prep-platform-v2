A `std::mutex` guarantees that only one thread writes at a time — it says nothing about
*which* waiting thread gets in next. Run the boilerplate against the tests before trusting
"assign a ticket atomically, then lock a mutex to write" — it fails almost every entry, not
occasionally. `std::mutex` does not implement first-come-first-served; a thread can easily
re-acquire a lock it just released before a different, longer-waiting thread gets scheduled.

---

You need each thread to wait for *its specific turn*, not just for exclusive access. Track
"which ticket gets to write next" as its own piece of state, and have each thread block
until that value equals its own ticket — `std::condition_variable`'s predicate form is
exactly built for "wait until this specific condition about shared state becomes true."

---

`cv.wait(lk, [&]{ return ticket == next_to_write_; })` — after writing, increment
`next_to_write_` and `notify_all()` (not `notify_one()`: every waiting thread has a
*different* ticket it's checking for, the same reasoning
[[019-fizzbuzz-multithreaded]]'s reading covers — only one of them will ever find its own
predicate true, but you can't know in advance which one the OS would pick with
`notify_one`).
