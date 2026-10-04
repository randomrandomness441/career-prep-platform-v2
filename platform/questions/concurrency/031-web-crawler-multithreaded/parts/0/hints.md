Get the pool working without worrying about exceptions first: a shared queue, a
`visited` set, a condition variable workers wait on for "queue non-empty or done." The
tricky part of *that* much is termination — `queue.empty()` alone can't mean "done," because
a worker that just popped its URL and unlocked to call `getUrls` hasn't reported back yet,
and it might add more work. Track how many workers are currently "active" (popped a URL,
haven't finished processing it) alongside the queue; only declare `done` when the queue is
empty **and** no worker is active.

---

Run the boilerplate against the tests before assuming the pool logic above is the whole
question — it crashes the entire process, not just one crawl. `getUrls()` is called with no
try/catch around it anywhere. An uncaught exception escaping a `std::thread`'s entry
function calls `std::terminate()`, which kills the process, not just the thread that threw.

---

Wrap the `getUrls()` call in `try`/`catch(...)`. The bookkeeping that follows it —
re-locking, decrementing the active-worker count, checking whether the pool is now done,
`notify_all()` — has to run whether that call succeeded or threw, because every other worker
is depending on that notification to ever wake up again. Structure it so both the success
and failure paths converge on the same "I'm done with this URL" code, rather than
duplicating it, or writing an early `return`/`continue` on the failure path that skips it.
