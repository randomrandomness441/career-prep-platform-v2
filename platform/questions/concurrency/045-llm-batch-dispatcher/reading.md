## 1. Reframe the problem

A GPU forward pass on a batch of 32 prompts costs barely more than a forward pass on one,
the hardware is built to move a lot of data through the same matrix multiplications at once,
and running requests one at a time throws that away almost entirely. So a real inference
server's job isn't "run this request," it's "collect requests from however many callers show
up, run them together, and hand each caller back exactly their own answer without them ever
knowing they were batched." The concurrency problem hiding inside that: many client threads
all trying to add themselves to the *same* pending batch at once, and a queue is exactly the
kind of shared, mutated state that needs the same care as any other, even though "just
append to a list" sounds too simple to need it.

## 3. The broken version, first

The boilerplate's `submit()` appends straight into the shared pending list, with nothing
guarding it:

```cpp
pending_.emplace_back(std::move(req), std::move(promise));
```

**Why it looks right:** it's the natural way to write "add this to the queue," and for a
single caller it's completely correct, the bug only exists because many threads are calling
this at once, and nothing about the line itself signals that. Run against the correctness
tests alone, and 300 concurrent clients still queue up cleanly:

```
solution.cpp -> CLEAN
boilerplate.cpp -> CLEAN (correctness stage only)
```

Only the full sweep, under ThreadSanitizer, shows the real problem:

```
WARNING: ThreadSanitizer: data race
 Read of size 8 by thread T2 ... previous write of size 8 by thread T1
 BatchDispatcher::submit(Request) solution.hpp:25
```

`std::vector::emplace_back` isn't just "write one new element", when the vector's capacity
is exhausted, it allocates a new, larger buffer and moves everything into it. Two threads
racing through that growth at the same moment are touching the same backing memory with no
coordination at all; this course's other boilerplates have mostly shown races that quietly
lose an update, but a concurrent vector reallocation is a much sharper kind of undefined
behaviour, the same shape of bug that would, on a different run or a different vector size,
just as easily crash instead of merely being flagged by TSan.

The fix is a mutex around the mutation:

```cpp
std::lock_guard<std::mutex> lk(m_);
pending_.emplace_back(std::move(req), std::move(promise));
```

Full sweep on the fix: `CLEAN`, 12/12 correctness, 20 clean TSan runs, 150 clean shaken
runs, all 300 concurrent clients queued and each resolved to exactly their own response.

## 6. Where this solution fails

- **`flush()` has to be called by something, this class has no policy for *when*.** A real
 server needs either a background thread calling `flush()` on a timer, or a check inside
 `submit()` itself that triggers a flush once `pending_count()` crosses some batch-size
 threshold, or (most realistically) both, flush on whichever comes first, a full batch or
 a max-wait timeout, so a lone request at 2am doesn't wait forever for 31 friends to show up.
 This exercise deliberately separates "the mechanics of batching" from "the policy of when,"
 and only builds the first.
- **One request in a batch throwing (a malformed input the model rejects) has no defined
 behaviour here**, `process_batch` returning a shorter vector than it was given, or
 throwing partway through, leaves some promises never fulfilled, which means their
 `future::get()` blocks forever. A production version needs a documented per-request
 failure path, a `Response` that can represent "this one failed," or catching an exception
 from `process_batch` and calling `set_exception()` on every pending promise in that batch
 (see [[048-exceptions-across-threads]]).
- **A caller who never calls `.get()` on their future just quietly leaks the promise's
 shared state until the dispatcher itself is destroyed**, not a bug exactly, but worth
 knowing: nothing about this design detects an abandoned request.
- **Every `submit()` call takes the same mutex, so under extreme request rates the queue
 itself becomes the contention point**, matching this course's earlier lesson
 ([[030-atomics-vs-mutexes]], [[038-multithreaded-memory-pool]]): at large enough scale, a
 sharded queue (multiple pending lists, requests hashed across them, each flushed and
 batched independently) trades a single global batch for less contention on the submit
 path.

## 7. Interview follow-ups

**"How would you decide the actual batch-flush policy, size threshold, time threshold, or
both?"** Both, racing each other: flush as soon as `pending_count() >= max_batch_size` (don't
make a full batch wait unnecessarily), or after `max_wait` has elapsed since the *oldest*
pending request arrived, whichever comes first (so a slow trickle of requests still gets
served promptly instead of waiting indefinitely for a batch that may never fill). The right
`max_wait` is a direct trade: larger batches are more GPU-efficient per request, but every
request pays the wait time as added latency, this is the batching server's version of
Amdahl's law, trading total throughput against any one request's response time.

**"A request in the batch causes process_batch to throw, walk through what has to happen for
every caller to get a sane result."** Every promise associated with that batch needs
`set_exception(std::current_exception())` rather than `set_value()`, inside a `catch` around
the `process_batch` call, mirroring [[048-exceptions-across-threads]]'s lesson that an
exception has to be explicitly relayed across the boundary between the thread that ran the
work and the thread that's waiting on it, or every one of those futures hangs at `.get()`
instead of surfacing the failure.

**"10^8 requests/sec target, what's the first thing that breaks in this design?"** The
single mutex guarding one shared `pending_` vector, every request pays for that lock
regardless of how much actual GPU work is happening. At that scale you'd shard the incoming
queue (multiple independent pending lists, each flushed on its own schedule, each backed by
its own model replica or a separate slice of GPU capacity) rather than trying to make one
mutex fast enough; the batching *logic* here stays the same per shard, only the "one shared
list" assumption has to go.
