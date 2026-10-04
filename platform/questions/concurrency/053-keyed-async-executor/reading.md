## 1. Reframe the problem

A plain thread pool answers "run many things concurrently." This question asks a more
specific thing: "run many things concurrently, *except* some of them have a relationship
with each other that concurrency would break." A per-key ordering requirement isn't a
special case of thread-pool design, it's a genuinely different contract, the pool's job
changes from "get to everything" to "get to everything, while never letting two related
things race." The natural first instinct, a normal pool, key passed along as metadata, is
tempting precisely because the signature *looks* like it should just work; the pool doesn't
know it's supposed to care about the key at all unless you build that in.

The shape this maps to in real systems: a microservice consuming events from a queue (Kafka,
SQS) where events are partitioned by some entity id specifically so a consumer *can* process
different entities in parallel, but the events for one entity still arrive, and must be
applied, in order. This exercise is that constraint, stripped down to its essence.

## 3. The broken version, first

The boilerplate is an ordinary thread pool that happens to accept a `key` parameter it never
looks at:

```cpp
void submit(std::string /*key*/, std::function<void()> task) {
    { std::lock_guard<std::mutex> lk(m_); q_.push(std::move(task)); }
    cv_.notify_one();
}
```

**Why it looks right:** the signature matches what was asked for, the pool genuinely does
run every task, and for a workload where no two tasks ever happen to share a key at the same
moment, this is indistinguishable from correct. The bug only exists in the specific case the
requirement is actually about.

Running it, 12 keys, 200 tasks per key, 8 workers:

```
26 times, two tasks for the SAME key ran overlapping -- per-key mutual
exclusion was violated
```

Two idle workers, both pulling from the one shared queue, happened to pick up two tasks for
the same key close enough together that both were mid-execution at once, exactly the
scenario a plain pool has no way to prevent, because it has no concept of "key" at all.

The fix, gate the shared ready queue so at most one dispatch per key is ever sitting in it:

```cpp
if (active_.insert(key).second) {
    ready_.push(make_dispatch(key)); // first task for this key: go now
}
// else: queued in pending_[key], picked up by the currently-running dispatch
```

Same 12-key, 200-task test: zero overlaps, zero ordering violations, every task completes.

## 6. Where this solution fails

- **One consistently "hot" key can starve the pool's usefulness for that key's own
 throughput**, even though other keys run fine, since a key's tasks are strictly
 serialized, a key that receives work faster than one worker can process it just backs up
 in that key's pending list, no matter how many idle workers the pool has. Per-key ordering
 and per-key throughput are in real tension: more parallelism can't help one overloaded key
 without breaking the ordering guarantee for it.
- **`pending_` and `active_` are unbounded**, nothing here limits how much work can be
 queued for a key before it's processed. A producer that submits faster than a key can
 drain grows memory without limit, the same backpressure gap
 [[024-backpressure]] and [[025-thundering-herd]] raise for a single shared queue, unaddressed
 here per-key.
- **A task that throws terminates the process** the same way an unguarded task does in
 [[032-work-stealing-pool]], nothing here catches an exception escaping a dispatched task,
 and it also means the key's own re-dispatch chain (the part of `make_dispatch` that would
 enqueue the *next* task for that key) never runs, silently stalling every later task for
 that key even if the process somehow survived.
- **The number of distinct keys is implicitly bounded by available memory**, not by
 `num_workers`, `pending_`/`active_` grow with distinct active keys, not with pool size.
 For a workload with a huge, unbounded key space (a key per individual request rather than
 per entity, say), this degenerates toward one pending-list entry per request, which isn't
 the shape this design is efficient for.

## 7. Interview follow-ups

**"What real technology already implements exactly this pattern?"** Kafka consumer groups,
partitioned by key, are the production version of this exact idea, messages for the same
key land on the same partition, and a partition is consumed by exactly one consumer instance
at a time, giving per-key ordering for free from the messaging layer, at the cost of that
partition's throughput being capped by one consumer. Akka/Erlang-style actors are another:
one actor per key, processing its own mailbox one message at a time, is the same guarantee
built from a different primitive.

**"How would you monitor this in production to catch the 'one hot key backs up' failure
mode?"** Track per-key pending-queue depth as a metric, alerting on any key whose backlog
grows past a threshold, a single overloaded key won't show up in aggregate pool metrics
(the pool overall can look perfectly healthy, idle workers and all, while one key's queue
grows unbounded) because the other 11 keys in this exercise's own test are proof that
everything else keeps working fine regardless.

**"If you needed strict ordering across ALL keys, not just within each key, would this
design still work?"** No, that's [[052-ordered-disk-log]]'s problem, not this one, and the
two are close to opposites: this design's entire value is letting different keys run out of
order *relative to each other*, trading a global order for parallelism. A true global-order
requirement collapses back to a single serialization point, the same one 052 builds with a
ticket and a condition variable.
