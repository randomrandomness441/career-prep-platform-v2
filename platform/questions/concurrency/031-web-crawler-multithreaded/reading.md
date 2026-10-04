## 1. Reframe the problem

A single-threaded crawler is a plain graph traversal. Pop a URL, fetch its links, push the
new ones, repeat until the queue's empty. Concurrency doesn't change the algorithm's shape.
It changes one question that used to have an obvious answer: **when are you done?** With
one thread, "the queue is empty" and "done" are the same fact. With several workers pulling
from the queue at once, they can come apart. A worker can pop the *last* URL, leaving the
queue empty, and still be off fetching its page. If every other worker looked at an empty
queue right then and declared victory, the URLs that fetch was about to discover would
never be visited. "Done" has to mean *the queue is empty and nobody is still working on
something that might refill it*. That's why the pool needs a live count of active workers
alongside the queue itself, not just the queue.

The second thing multithreading changes: a real fetch (`getUrls`) can fail, and a raw
`std::thread` has a specific, sharp opinion about failure. An exception that reaches the top
of a thread's entry function without being caught calls `std::terminate()`, which ends the
*process*, not just that one thread's work. A crawler that's correct for the happy path and
silent about this is one flaky page away from taking down the whole crawl.

## 3. The broken version, first

The boilerplate calls `getUrls()` with nothing around it:

```cpp
std::vector<std::string> urls = parser.getUrls(url); // may throw
```

**Why it looks right:** it's a direct translation of the pool design, which is genuinely
correct. The queue, the `visited` set, the `active_workers` count, and the termination
check are all sound. Exception safety is easy to leave for "later" specifically because
nothing about the *concurrency* logic is wrong. The gap is a completely separate concern
that happens to sit on the same line.

Running it against a page that throws:

```
crawl() let a page's exception escape a worker thread: the process was killed by
signal 6 (Abort trap: 6), consistent with std::terminate().
libc++abi: terminating due to uncaught exception of type std::runtime_error:
simulated network error
```

Not a hang, not a wrong answer. The whole process is gone. Every URL that any other worker
had successfully discovered is lost with it, not just the one page that actually failed.

The fix is a `try`/`catch(...)` around the call, with the active-worker bookkeeping made to
run on both the success and failure paths:

```cpp
try { urls = parser.getUrls(url); }
catch (...) { ok = false; }
// active_workers--, done-check, and notify_all() happen either way
```

Same throwing page, now:

```
a throwing page doesn't crash or hang the pool, and 8 trials of the reachable-set
check plus a clean no-failure run all matched exactly
```

Every URL reachable through other pages is still found. Only the failing page's own,
unknowable outbound links are missing from the result. That's the correct, honest answer,
not a crash.

## 6. Where this solution fails

- **A page that fails is silently dropped, with no signal to the caller about which one or
 why.** This question's contract only requires the crawl to *complete*. A production
 crawler almost certainly wants to know which URLs failed and why, since a 404, a timeout,
 and a malformed response are different problems. That means collecting failures alongside
 successes, not just swallowing them.
- **A page that *hangs* instead of throwing isn't handled by this fix at all.** `try`/`catch`
 only helps once `getUrls` actually returns, by exception or by value. A call that never
 returns, a stuck network request with no timeout, ties up one worker thread forever, the
 same problem [[041-deadlock-detection-watchdogs]] is built around. A production crawler
 needs a per-fetch deadline, independent of this fix.
- **`visited` grows without bound for a crawl with no natural edge, and nothing here caps
 it.** A site with effectively infinite generated URLs, calendar pages, session-ID query
 parameters, never terminates on "the queue is empty." It terminates when memory runs out.
 Real crawlers cap depth, total pages, or both.
- **The lock is held for every queue and `visited` operation, but never for the network call
 itself.** That's deliberate, the whole point of `lk.unlock()` before `getUrls()`, but it
 means `visited.insert()` can race two workers discovering the same new URL from different
 pages at almost the same moment. Only one of them wins the insert and queues it, which is
 correct, but a design that wanted per-page metadata, like first-seen time or referring
 page, needs to handle "I found it, but someone else queued it first" as a real case, not
 an edge case to ignore.

## 7. Interview follow-ups

**"Why track active_workers instead of just checking the queue?"** Because "the queue is
empty" is true for two different reasons that need different responses: either the crawl is
genuinely finished, or a worker just took the last item and hasn't reported back yet. Only
the count of workers currently mid-fetch distinguishes them. `queue.empty() && active == 0`
is the actual termination condition; `queue.empty()` alone is a race.

**"What if getUrls() itself spawns more work asynchronously instead of blocking, does this
design still apply?"** The core idea, track outstanding work not just queued work, and don't
declare done until both are zero, generalizes, but the mechanism changes. If `getUrls`
returns a future or hands off to a callback instead of blocking the calling worker, you need
a count of *outstanding async operations*, not busy worker threads. That's closer to a
[[050-shared-future-fanout]]-style fan-out/fan-in than a blocking thread pool.

**"How would you add a per-fetch timeout, given this design?"** Wrap the `getUrls()` call
the way [[041-deadlock-detection-watchdogs]]'s `run_with_deadline` wraps an arbitrary
operation. Run it on its own thread, or use whatever async facility your real HTTP client
offers, and race it against a deadline, treating a timeout the same way this fix treats a
thrown exception: log it, run the cleanup bookkeeping, move on. The two problems, an
exception versus a hang, need different detection mechanisms but the same response.

**"10^8 URLs, need this to scale past one process, what changes structurally?"** The
`visited` set and the queue both need to become distributed, a shared key-value store for
dedup, a distributed queue like Kafka or SQS for work. "active_workers" as an in-process
counter stops making sense too; you'd track outstanding work via the queue system's own
in-flight/acknowledgment tracking instead. The single-process design here is really a
special case where dedup and coordination both fit in one machine's memory. The algorithmic
shape, discover, dedup, enqueue, track outstanding work, carries over. The mechanism for
each piece doesn't.
