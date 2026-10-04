A plain thread pool — one shared queue, N workers popping from it — gives you "everything
eventually runs, on N threads," but nothing about *which* worker picks up *which* task. Run
the boilerplate against the tests before trusting `key` can just be ignored: two tasks for
the same key, submitted close together, can easily be popped by two different idle workers
at nearly the same instant.

---

The invariant you need: at any moment, **at most one task for a given key is either running
or waiting in the ready-to-run queue.** A key's second task shouldn't go straight into the
shared queue at all — it should wait in a per-key holding area until the key's first task
has actually finished.

---

Track two things per key: a small pending list (tasks for this key that haven't started
yet) and whether this key currently has anything "in flight." `submit` appends to the
pending list; only if the key had *nothing* in flight does it also push a unit of work into
the shared ready queue. That unit of work, when it runs, does one task, then checks the
key's pending list again — if there's more, it enqueues the *next* dispatch for this same
key; if not, it marks the key idle. This way, the shared queue never has two ready-to-run
units for the same key at once, without ever holding a lock across an actual task's
execution.
