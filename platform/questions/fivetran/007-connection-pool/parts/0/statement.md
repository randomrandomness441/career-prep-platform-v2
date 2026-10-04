A connector holds a small pool of DB connections instead of opening a fresh one per query.
Multiple worker threads lease and release connections concurrently, so the pool itself has
to be thread-safe -- no two threads may ever be handed the same connection at once, and the
pool must never create more connections than its configured limit.

Implement a class `Solution` with:

    Solution(ConnectionFactory factory, int maxSize)
    MockConnection lease() throws InterruptedException
    void release(MockConnection conn)

`lease()` hands back an idle connection if one is available, creates a new one via
`factory.create()` if the pool hasn't reached `maxSize` yet, or blocks until another
thread calls `release()` if it has.

`release(conn)` returns a connection to the pool for reuse -- unless it's gone bad, in
which case close it and don't hand it out again.

**Requirements**

- Health check on every path a connection could be reused: before handing an idle
  connection out of `lease()`, and when a connection comes back into `release()`, check
  `conn.isValid()`. An invalid connection gets `conn.close()` called on it and is evicted,
  never returned to a caller again.
- Never exceed `maxSize` connections outstanding (created but not yet released) at once.
- `lease()` blocks (doesn't busy-spin, doesn't throw) when the pool is at `maxSize` and
  nothing idle is available, until a `release()` makes room.
- Safe under concurrent `lease()`/`release()` calls from multiple threads -- this is
  tested with a real multi-threaded stress test, not just single-threaded correctness.
