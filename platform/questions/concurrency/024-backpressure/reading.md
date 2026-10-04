# Backpressure: What to Do When the Consumer Is Slower

## 1. Reframe the problem

This is not a question about queues. It is the question every system with a faster front
end than back end must answer: **when the machine cannot keep up, who finds out, and how?**

The naive answer is "nobody finds out", the queue accepts everything and grows. That
answer has a name in production: the OOM kill. It always looks the same in the post-mortem:
the service was fine for hours, then the consumer hiccupped (a slow disk, a GC pause, a
network blip), the queue absorbed the difference silently, and memory climbed until the
kernel chose a victim. No error was ever returned, so no metric ever moved, so nobody was
watching.

The reframe: **capacity is a contract about memory, and the overflow policy is a contract
about who absorbs the pain when memory would be exceeded.** `Block` says the *producer*
absorbs it (its latency grows). `DropNewest` says the *newest request* absorbs it (its
caller sees a refusal). `DropOldest` says the *stalest data* absorbs it (fine for
telemetry, catastrophic for commands). Choosing a policy is a product decision disguised as
an implementation detail, and the senior-engineer answer is that whichever you choose,
**the drop must be counted**, because silent data loss is the only genuinely unacceptable
option.

## 3. The broken version, first

The naive version is seductive for the best reason: it is *simpler and it works*. `push()`
always succeeds, no error path, no return value to check, no waiting, and in every demo,
every benchmark, every code review on a Tuesday, the consumer keeps up and the bound never
bites. It "seems correct" because unboundedness is invisible in every run where the system
is healthy, and systems are healthy almost all the time. The failure arrives on the one
day the consumer falls behind, and it arrives as an OOM kill rather than as a bug report.

Here is the actual naive code path, note what is missing:

```cpp
bool push(T v) {
    std::unique_lock<std::mutex> lk(m_);
    if (closed_.load()) return false;
    q_.push_back(std::move(v)); // capacity_ stored, never consulted
    lk.unlock();
    not_empty_.notify_one();
    return true;
}
```

Run it against the harness and the failure is immediate and deterministic, **12 of 12
runs**:

```
tests fail, failed 12/12 runs

pushed 10 items into a queue of capacity 4 and it is holding 10 of them.
That is an unbounded queue: nothing here limits memory, and a producer that
outruns its consumer will grow it until the process is killed.
```

Ten pushes into a capacity-4 queue, and all ten are in there. The test checks the *bound*
itself, which is exactly the thing the naive version never enforces. The fix is the four
TODOs: a second condition variable (`not_full_`) so producers and consumers wake for
different reasons, the three policies in `push`, a `dropped_` counter incremented on every
discarded item, and a `not_full_.notify_one()` in `pop()`, because a consumer taking an
item out is news to a producer blocked on a full queue.

That last one is the subtle production lesson: the naive version's `not_empty_` is a
one-way street. Real backpressure is a *round trip*, the consumer's progress must be
signalled back to the producer, or a Block policy deadlocks the moment the queue fills.

## 6. Where this solution fails

- **`Block` on a single consumer that has died.** A blocked producer waits forever unless
 `close()` is called. In production, "the consumer crashed" must route to `close()` or you
 have traded an OOM for a hang. Anything you block on needs a shutdown story and,
 ideally, a timeout story (`wait_for` + health check).
- **`DropOldest` on command streams.** Evicting the *oldest* item is right for telemetry
 ("my newest sample matters") and exactly wrong for commands ("process my transfers in
 order"). `DropOldest` on a payment queue silently drops the oldest pending payment,
 and, because it returns `true`, the caller never hears about it.
- **`dropped()` is a metric, not a cure.** Counting drops does not make them okay. The
 production loop is: count → alert on the rate → shed load *upstream* (reject at the edge,
 return 503, shed the cheapest requests) so the drop lands on a caller who can retry, not
 on data already inside your process.
- **`notify_one` with many consumers is still a herd.** With C consumers, `pop`'s notify
 wakes one, but `push`'s `not_empty_.notify_one()` wakes one consumer per item, which is
 right; the failure mode returns when you `notify_all` on every push (see 025). Under
 hundreds of blocked producers, one `not_full_.notify_one()` per pop also serialises
 producer wakeups through the mutex, at high producer counts a per-producer semaphore or
 a ticket design (025's solution) wakes exactly one.
- **The bound is on items, not bytes.** A capacity of 4 holds four `std::string`s that
 might be 16 bytes or 16 megabytes. Memory contracts in production are denominated in
 bytes with a deadline ("no more than 64 MiB buffered, no longer than 500 ms"), a
 size-and-age-bounded queue is the production shape, and neither `std::queue` nor this
 class gives you the age half.
- **Correct but too slow:** the mutex serialises producers and consumers on one lock. At
 millions of items per second this queue is the bottleneck and the answer is a different
 structure (016's SPSC ring between exactly two threads, or a Michael–Scott queue,
 049), backpressure semantics layered on top, not a faster mutex.

## 7. Interview follow-ups

**Q. Small machine, one core, the consumer and producer must share it. What changes?**
`Block` becomes dangerous: a producer spinning or sleeping while the consumer wants the CPU
adds scheduler ping-pong to your latency problem. Under co-scheduling, you also want the
producer to *stop earlier*, the useful capacity is smaller than on a big box, because
burst absorption now costs the consumer CPU time. And any spin-then-block hybrid must lean
block, because a spin wastes the only core the consumer has.

**Q. Big machine, 64 cores, 128 producers. Where does this design fall over first?**
On the mutex. Every producer and consumer funnels through one `std::mutex` guarding one
deque: you have built a single-lane bridge on a 64-lane highway. First symptom is not
correctness, it is `sys%` time and cache-line ping-pong on the lock word as all 128
producers' cores invalidate each other. The scalable shape: per-producer or per-core
queues (sharding), a bounded MPMC ring per shard, or per-producer semaphores for wakeup.
The *semantics* you built here survive; the *structure* does not.

**Q. 10^8 events/second. Which resource saturates first?**
Not the mutex, you left that behind several orders of magnitude earlier. The order is:
(1) the wakeup path, a futex wake + context switch per item is microseconds, and 10^8/s
of them exceeds whole-core budget; production systems batch (wake one consumer for N items
via a sequence counter, not per item); (2) memory bandwidth for the payload copies,
move, don't copy, and keep items in L2-sized rings; (3) the drop counter itself, if
`dropped_` is one shared atomic incremented on every event under sustained overload, pad
it, shard it, or sample it. This is why serious ingest systems (Kafka, DPDK apps, trading
gateways) are all: per-core rings + batch handoff + counting in bulk.

**Q. Failure injection: an exception flies inside the consumer's processing of a popped
item. What does the queue do?**
Nothing, correctly. The item is already out; the queue is consistent; the exception
belongs to the consumer's policy (retry? dead-letter? crash?). The dangerous variant is an
exception thrown *inside `push` while blocked* (a `Block` waiter woken by spurious wakeup
into a `std::bad_alloc` from the copy), RAII on the lock handles the mutex, but the
caller must know the item was never accepted. This is why `push` returning `bool` beats
`void` at senior level: every failure path has a reportable outcome.

**Q. Client disconnects mid-stream in a server using this queue. How do you avoid queue
poisoning?**
Tag work with an id and validate on pop: if the response can no longer be delivered, drop
the item *after* popping (cheap) rather than trying to remove it from the middle of the
queue (impossible in O(1)). Never block the producer on behalf of a dead consumer: the
disconnect is a signal to shed that client's queued work, which is exactly what a bounded
queue + drop policy does for free.

**Q. Production ops: which metrics do you watch on a bounded queue?**
Depth (against capacity, sustained 90%+ means you are about to drop or block), drop rate
(the one that pages you), producer block time (latency the queue is imposing), consumer
lag (items behind the head), and close-to-drained time (shutdown health). The senior tell:
*rate* of drop matters more than *count*, a million drops during a 10-second incident is
one story, a thousand drops per minute for an hour is a different, worse one.
