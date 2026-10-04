A sync engine runs connector jobs off a shared queue. Higher-priority jobs (a customer-
triggered manual sync) should jump ahead of routine scheduled ones, a failed job should get
retried with backoff instead of hammering the source again immediately, and a job that
keeps failing should eventually get dropped instead of retrying forever. Multiple worker
threads pull from the same queue at once.

Implement a class `Solution`:

    Solution(int maxAttempts, long baseBackoffMillis)
    void submit(Job job)
    Job poll(long nowMillis)
    void fail(Job job, long nowMillis)
    void complete(Job job)

`Job` has `id` and `priority` (higher sorts first). `poll` and `fail` take the current
time explicitly instead of reading the clock themselves, so behavior is deterministic to
test.

- `submit` adds a brand-new job, ready immediately.
- `poll(nowMillis)` removes and returns the highest-priority job that's ready to run at
  `nowMillis` (ties broken by submission order, earliest first). Returns `null` if nothing
  is ready right now -- it does not block.
- `fail(job, nowMillis)` reports that a polled job failed. If it still has attempts left
  (this was attempt number `< maxAttempts`), re-enqueue it with exponential backoff:
  `readyAt = nowMillis + baseBackoffMillis * 2^(attemptNumber - 1)`. Once it's used up
  `maxAttempts` attempts, drop it -- it never comes back from `poll` again.
- `complete(job)` reports a polled job finished successfully; just clears its internal
  bookkeeping.

**Requirements**

- Safe under concurrent `submit`/`poll`/`fail`/`complete` calls from multiple threads --
  this is tested with a real multi-threaded producer/consumer stress test, not just
  single-threaded correctness.
- No job is ever delivered to two callers at once, and every submitted job is eventually
  delivered exactly once (assuming it isn't retried -- a job that fails and gets
  re-enqueued is expected to come back out of `poll` again, up to `maxAttempts` times).
