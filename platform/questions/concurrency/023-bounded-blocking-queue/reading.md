## 1. Reframe the problem

A condition variable is not a flag and it is not a signal you can save up. It is a
**waiting room with a door**. Threads go in and sleep. `notify_one` opens the door for one
sleeper; `notify_all` opens it for everyone. If nobody is inside, opening the door does
nothing at all and the notification is simply gone, there is no memory of it.

Once you hold that picture, the bounded queue asks one question that has nothing to do with
queues: **how many waiting rooms do I need?**

A producer parks because *the queue is full*. A consumer parks because *the queue is empty*.
Those are two different statements about the world. They become true at different moments,
for different reasons, and they matter to different threads. A producer woken because "the
queue is no longer empty" has learned nothing it can act on, it goes straight back to
sleep.

So the answer is two. Two conditions, two waiting rooms:

```
not_full_ <- producers sleep here. dequeue() wakes one.
not_empty_ <- consumers sleep here. enqueue() wakes one.
```

Every notification then lands on a thread that was waiting for precisely the change that
just occurred. That is what makes `notify_one` safe to use. Put both kinds of sleeper in
one room and `notify_one` is a coin flip: you meant to wake a consumer and you woke a
producer, who checks its own predicate, finds nothing has changed for *it*, and lies back
down, having absorbed the only notification anyone was going to send.

LeetCode 1188 lets you get away with the coin flip because its test is one producer and one
consumer, so the room only ever contains one kind of sleeper. Add a second producer and a
second consumer and the same code stops forever.

There is a second problem the LeetCode version does not have. Real queues get shut down,
and shutdown happens while threads are asleep inside `wait()`. A sleeping thread cannot
poll a flag; it only re-evaluates its predicate when someone notifies it. So "stop" has to
be expressed as *another reason for the predicate to become true*, plus a notification loud
enough to reach everybody. That is the `closed_` flag, and it must appear in **both**
predicates, because there are threads asleep in both rooms.

## 2. The tools

### `wait` with a predicate, always

```cpp
std::unique_lock<std::mutex> lk(m_);
cv_.wait(lk, [this] { return !q_.empty(); });
```

Three things happen inside `wait`, atomically with respect to other threads holding `m_`:

1. It checks the predicate. **If it is already true, it does not sleep at all.**
2. Otherwise it releases `m_` and parks the thread, as one indivisible step, which is why
 the lock must be passed in.
3. When woken, it reacquires `m_` and re-checks the predicate. Still false? Back to sleep.

That third point is not optional politeness. Condition variables are permitted to wake you
for no reason, a *spurious wakeup*, and on top of that another thread may have taken the
item between your wakeup and your reacquiring the lock. The predicate overload is exactly
this loop, written for you:

```cpp
while (!pred()) cv_.wait(lk); // what the predicate overload compiles to
```

Never write the bare `cv_.wait(lk)` unless you have written that loop yourself.

Note that `std::unique_lock` is required rather than `std::lock_guard`: `wait` has to
unlock and relock, and `lock_guard` has no interface for that.

### `notify_one` vs `notify_all`

```cpp
cv_.notify_one(); // wake one arbitrary sleeper (unspecified which)
cv_.notify_all(); // wake all of them
```

`notify_one` is correct when **any one** of the sleepers can consume the change and all
sleepers are interchangeable. One item arrived; one consumer can take it; all consumers
want an item. Fine.

`notify_one` is *wrong* when the sleepers are not interchangeable, which is exactly what
happens when two conditions share a variable. That is the bug in section 3.

You do not need to hold the mutex to call either one. You *do* need to hold it while
changing the state the predicate reads, otherwise the check-and-park inside `wait` can slip
into the gap between your write and your notify.

### Unlock before notify

```cpp
q_.push(std::move(v));
lk.unlock(); // release first
not_empty_.notify_one(); // then wake
```

If you notify while still holding `m_`, the thread you wake comes out of `wait`, tries to
reacquire `m_`, finds it held by you, and goes back to sleep on the mutex. It gets woken
twice to do one thing. The pattern has a name, *hurry up and wait*, and section 5 has the
measurement.

### The full skeleton

```cpp
bool enqueue(int v) {
    std::unique_lock<std::mutex> lk(m_);
    not_full_.wait(lk, [this] { return q_.size() < capacity_ || closed_.load(); });
    if (closed_.load()) return false;
    q_.push(std::move(v));
    lk.unlock();
    not_empty_.notify_one();
    return true;
}

std::optional<int> dequeue() {
    std::unique_lock<std::mutex> lk(m_);
    not_empty_.wait(lk, [this] { return !q_.empty() || closed_.load(); });
    if (q_.empty()) return std::nullopt; // closed AND drained
    int v = std::move(q_.front());
    q_.pop();
    lk.unlock();
    not_full_.notify_one();
    return v;
}

void close() {
    { std::lock_guard<std::mutex> lk(m_); closed_.store(true); }
    not_full_.notify_all();
    not_empty_.notify_all();
}
```

Read `dequeue`'s exit test once more. It returns `nullopt` when the queue is **empty**, not
when it is *closed*. A closed queue keeps handing out what is already inside it; only when
it is closed and drained does the consumer see end-of-stream. Getting that backwards
silently discards whatever was buffered when shutdown began.

### `std::optional` as the end-of-stream marker

`dequeue` cannot return `T` by value, because after `close()` there may be no `T` to
return. It cannot signal the difference by exception without making a normal shutdown cost
a throw per consumer. `std::optional<int>` says "a value, or nothing" in the type, so the
caller's loop is the natural shape:

```cpp
while (std::optional<Job> j = q.dequeue()) run(*j);
```

### One type detail, because the compiler will stop you

`q_.size()` is a `std::size_t`, unsigned. If you declare the capacity as `int`, then

```cpp
q_.size() < capacity_ // size_t vs int
```

is a signed/unsigned comparison. `-Wall -Wextra` rejects it, and it is not pedantry: the
`int` is converted to `size_t`, so a negative capacity becomes about 18 quintillion and the
bound disappears entirely. Store the capacity as `std::size_t` and the problem cannot
occur.

## 3. The broken version, first

Here is what almost everyone writes, and it is not obviously wrong:

```cpp
class naive_queue {
    std::mutex m_;
    std::condition_variable cv_; // ONE waiting room
    std::queue<int> q_;
    std::size_t cap_;
public:
    void enqueue(int v) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return q_.size() < cap_; });
        q_.push(std::move(v));
        cv_.notify_one();
    }
    T dequeue() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return !q_.empty(); });
        int v = std::move(q_.front());
        q_.pop();
        cv_.notify_one();
        return v;
    }
};
```

Both predicates are correct. Both waits use the predicate overload. Every access to `q_` is
under the mutex, so there is no data race, ThreadSanitizer has nothing to say about this
code. It is perfectly synchronised.

Four producers, four consumers, capacity 2, 200 items each. Actual output:

```
after 1 second(s): produced 155 of 800, consumed 153, queue holds 2
after 2 second(s): produced 155 of 800, consumed 153, queue holds 2
after 3 second(s): produced 155 of 800, consumed 153, queue holds 2
STUCK -- 4 producers parked on "not full", 4 consumers parked on "not empty", nobody left to notify anyone.
```

A second run:

```
after 1 second(s): produced 6 of 800, consumed 6, queue holds 0
after 2 second(s): produced 6 of 800, consumed 6, queue holds 0
after 3 second(s): produced 6 of 800, consumed 6, queue holds 0
```

Six items. The program ran for a few microseconds and then stopped being a program. Note
how the two runs disagree wildly, 155 items one time, 6 the next, while agreeing
completely on the outcome. That is the signature of a lost wakeup: *when* it happens is
random, *whether* it happens is not. Under the platform's harness the same code is killed
by the watchdog on every single run.

### The exact interleaving

Take the second run, capacity 2, and follow it.

1. Consumers C1–C4 start first, find the queue empty, and all four park on `cv_`.
2. Producer P1 pushes an item and calls `notify_one`. The waiting room contains four
 consumers, so a consumer is woken. Good.
3. Producers keep pushing. The queue reaches capacity 2. P2, P3, P4 now park on `cv_` too.
 **The waiting room now holds consumers and producers together.**
4. A consumer pops an item. Queue size drops to 1. It calls `notify_one`.
5. That notification wakes… whoever the implementation picks. Say it picks consumer C3.
6. C3 wakes, reacquires the mutex, evaluates *its* predicate: is the queue non-empty? The
 other consumers have already drained it. No. C3 parks again.
7. The notification is spent. The producers parked in step 3 were never told that room
 appeared. Nobody else is running to tell them.

Everybody is asleep, the door is shut, and no thread exists that could open it. This is not
a deadlock in the textbook sense, there is no cycle of held locks, and no lock is held at
all. It is a **lost wakeup**: a signal delivered to a thread that had no use for it.

`notify_one` promises to wake *a* thread. It does not promise to wake a *useful* thread.
When one waiting room holds two kinds of sleeper, "a thread" and "a useful thread" stop
being the same thing.

### The fix that looks like a fix

You can make the broken version work by changing `notify_one` to `notify_all`. Every
notification then reaches every sleeper, so the producer always hears about the room. This
is correct, and it is what a lot of production code does.

It is also the wrong lesson. `notify_all` on a shared variable wakes N threads so that one
of them can proceed; the other N−1 wake up, contend for the mutex, fail their predicate,
and go back to sleep. That is a thundering herd, and it is a separate question in this
course. Two condition variables get you the correctness *without* the herd, because you
never wake a thread that cannot use the news.

## 4. Real-world usage

**Why the bounded buffer was invented.** Dijkstra described the producer-consumer problem
with a bounded buffer in 1965, in the same work that introduced semaphores, and the reason
the buffer is *bounded* is the interesting part. An unbounded buffer between a fast producer
and a slow consumer is not a queue; it is a memory leak with a scheduling policy. The bound
is what converts "the consumer is falling behind" from an invisible, unbounded resource
consumption into a visible event: the producer blocks. That is backpressure, and it is the
next question in this set.

Hoare's monitors (1974) generalised this into "one condition variable per condition you
wait for", which is exactly the rule this question teaches. The two-condition-variable
bounded buffer is the canonical monitor example, and it has been the canonical example for
fifty years because everyone gets it wrong the first time.

**Where you meet it in production:**

- **Thread pools.** The task queue in every pool is this class. `enqueue` is `submit`,
 `dequeue` is what each worker sits in, and `close` is what `~pool()` calls before joining.
- **Logging.** Application threads enqueue formatted lines; one writer thread drains to
 disk. The capacity is what stops a logging storm from eating the heap, and the
 `close`-then-drain behaviour is what makes the last few log lines survive a shutdown,
 which is usually the only reason you wanted them.
- **Pipeline stages.** Decode → transform → encode, with a bounded queue between each pair.
 The bounds make the pipeline self-regulating: the slowest stage sets the rate for
 everything upstream of it, automatically, with no coordination.
- **Network servers.** The accept loop pushes connections; workers pop them. When the queue
 is full the accept loop blocks, the kernel's listen backlog fills, and clients see a
 connection refused rather than a server that has swallowed a million sockets.
- **Go channels, Java's `ArrayBlockingQueue`, Rust's `sync_channel`.** All of them are this
 class. `ArrayBlockingQueue` uses precisely the two-condition design, its fields are
 literally named `notEmpty` and `notFull`.

**Where NOT to use it:**

- **Not for one producer and one consumer at high rate.** A mutex-and-condition-variable
 queue costs a full sleep/wake round trip per handoff whenever the queue runs empty. An
 SPSC ring buffer with two atomic indices does the same job with no locks at all and
 roughly an order of magnitude more throughput. If the thread count on each side is
 exactly one and fixed, use the ring buffer.
- **Not as a general "async" primitive.** If consumers must wait for *specific* items, or
 results need to be routed back to particular callers, you want futures, not a queue.
 Threading a request id through a queue and matching replies by hand reinvents a worse
 `std::future`.
- **Not where blocking is unacceptable.** A real-time audio callback, a signal handler, or
 a thread holding another lock must not park here. Give those callers `try_enqueue` and a
 documented drop policy, a blocked audio thread produces an audible glitch, which is
 strictly worse than a dropped sample.
- **Not with an enormous capacity "to be safe".** A capacity of a million is an unbounded
 queue wearing a disguise. It hides the fact that the consumer is too slow until the day
 memory runs out, and the latency of an item that sits behind 999,999 others is useless
 anyway. Pick the smallest bound that keeps the consumer busy.

## 5. Performance

All measured on this machine, Apple M5, 10 cores, clang 21, `-O2`. Four producers, four
consumers, 200,000 items total, best of five runs, repeated across several passes.

**Unlock before notify, or not.** Same class, same two condition variables, one line moved:

| | ms for 200k items | ns per item |
|---|---|---|
| `lk.unlock(); cv.notify_one();` | 43–56 | 216–280 |
| `cv.notify_one();` with the lock still held | 79–104 | 393–520 |

**Holding the lock across the notify costs +73% to +110%**, five separate passes, all in
that range. The woken thread returns from `wait`, immediately blocks on the mutex, and has
to be woken a second time when the notifier finally releases it. Two wakeups per handoff
instead of one.

It is not universal, and the shape of the exception is the interesting part:

| configuration | unlock first | hold the lock | difference |
|---|---|---|---|
| 4 producers / 4 consumers, capacity 64 | 216 ns/item | 393 ns/item | +82% |
| 1 producer / 8 consumers, capacity 1024 | 283 ns/item | 326 ns/item | +15% |
| 1 producer / 1 consumer, capacity 1 | 7493 ns/item | 7432 ns/item | −0.8% |

With one thread on each side there is nothing to contend for, so the reordering buys
nothing and the difference disappears into the noise. **If you benchmark this advice with
one producer and one consumer you will conclude it does not matter, and you will be
generalising from the one configuration where it cannot matter.** The cost appears with
contention, which is the case you actually care about. Unlocking first is free, so do it
unconditionally.

(The 7.5 µs per item in the last row is not a defect of the queue. Capacity 1 forces a full
sleep/wake round trip for *every single item*, the two threads can never run at the same
time. That is the price of a condition-variable handoff on this machine, and it is the
number that justifies both larger capacities and lock-free SPSC rings.)

**Capacity is the dominant variable.** Four producers, four consumers, unlock-then-notify,
three passes:

| capacity | ns per item |
|---|---|
| 1 | 5100–10000 |
| 4 | 2300–3800 |
| 16 | 930–1280 |
| 64 | 320–373 |
| 256 | 144–160 |
| 4096 | 91–116 |

Roughly fifty times faster from capacity 1 to capacity 256. Every unit of capacity is slack
that lets a producer keep working while a consumer is busy; with no slack, every item pays
for a park and a wakeup. The curve flattens hard after a few hundred, because by then the
queue almost never actually reaches either bound and the only remaining cost is the mutex.
**Small capacities are for backpressure and latency, not for throughput**, if you chose 1
for memory reasons you paid 50× for nothing.

**`notify_one` vs `notify_all`, on the correct two-variable design:** 254 ns/item vs 292,
and 280 vs 305 on a second pass, **+9% to +15%** for `notify_all`. Modest here, because
each waiting room holds at most four threads. Scale the waiter count and this is the cost
that grows; that is the thundering-herd question.

## 6. Where this solution fails

- **`close()` while producers still have work is a data-loss decision, and this design
 makes it silently.** `enqueue` returning `false` means the item was never accepted, and
 the caller has to do something about that. Most callers written against this API ignore
 the return value, so a shutdown drops the last few items with no diagnostic. If losing
 them is unacceptable you need a two-phase shutdown: stop accepting new work, wait for the
 queue to drain, *then* close.

- **`size()` is a number that was true once.** By the time it returns, other threads have
 changed it. `if (q.size() < q.capacity()) q.enqueue(x);` is a check-then-act race: the
 space you observed can be gone before you use it. This is not a fixable flaw in the
 implementation, it is the interface race from chapter 3, and the reason `try_enqueue`
 exists as a single atomic operation. Treat `size()` as a metric, never as a control input.

- **No fairness, at either end.** Nothing stops the same consumer from winning the mutex
 repeatedly while another starves. `std::mutex` makes no fairness promise, and neither does
 `notify_one` about which sleeper it picks. Both are strongly *barging* in practice, a
 thread already running usually beats a thread being woken, so under sustained load you
 can see one consumer take nearly everything. If your items are per-tenant work, that is a
 fairness bug in your product. The fix is not a better queue; it is one queue per class of
 work plus a scheduler over them.

- **Priority inversion.** A low-priority producer holding the mutex blocks a high-priority
 consumer for as long as its critical section runs. `std::mutex` has no priority
 inheritance. On general-purpose servers this is invisible; in anything soft-real-time it
 is the reason your p99.9 has a cliff in it.

- **The mutex is one lock for the whole queue, so it is the ceiling.** Every operation on
 both sides serialises on `m_`. Beyond roughly four to eight threads total this stops
 scaling and starts going backwards, the cache line holding the mutex and the head/tail
 bounces between cores on every operation. The measurements above sit near the sweet spot,
 not on the flat part of a scaling curve. Past that you need multiple queues (one per
 worker, with work stealing) rather than a faster single queue.

- **`T`'s move constructor runs while the lock is held.** `q_.push(std::move(v))` and the
 `std::move(q_.front())` in `dequeue` are inside the critical section. If `T` is expensive
 to move, or worse, if its move constructor can throw or can take another lock, you have
 put arbitrary user code inside your mutex. Queue cheap-to-move handles (`unique_ptr`,
 small structs), not fat objects.

- **An exception from `q_.push` leaves the state consistent but the item lost.** If the
 underlying container throws `bad_alloc` while growing, `enqueue` propagates it, the
 `unique_lock` unlocks correctly, and no notification is sent, which is right, since
 nothing was added. The caller's item is destroyed by unwinding. Correct, but only if the
 caller expected `enqueue` to be able to throw.

- **Deadlock is still possible one level up.** If a consumer's handler enqueues back into
 the same queue and the queue is full, the consumer blocks in `enqueue` while being the
 only thread that could have made room. Bounded queues plus cycles in the dataflow graph
 equals deadlock, every time, and no amount of correctness inside the queue prevents it.
 Either break the cycle or use `try_enqueue` on the back edge.

- **The atomic `closed_` flag is not what makes `close()` correct.** Making it atomic lets
 you read it without the lock, which is convenient. But if `close()` sets it *without*
 taking the mutex, you reintroduce a lost wakeup: a thread evaluates its predicate as
 false, and before it can park, `close()` stores the flag and calls `notify_all` into an
 empty waiting room. The thread parks a moment later and never wakes. The lock around the
 store is load-bearing. Atomics protect values; they do not protect the check-then-park
 window inside `wait`.

- **Capacity 0 is quietly turned into 1.** A queue that can never have room would block
 every `enqueue` forever, so the constructor refuses to build one. What a caller passing 0
 probably wanted is a *rendezvous* channel, where the producer waits until a consumer
 actually takes the item, a different structure with a different protocol, not a bounded
 queue with a smaller number.

- **Capacity is fixed at construction, and it is the wrong knob at both ends.** Too small
 and throughput collapses (the 50× above). Too large and it stops providing backpressure
 and starts providing latency. There is no single right answer, because the correct
 capacity depends on the ratio of producer to consumer rates, which changes with load.
 Systems that need to get this right measure queue depth and adapt, which this class
 cannot do.

## 7. Interview follow-ups

**"Why is the atomic closed_ flag's own atomicity not what makes close() correct, isn't
that exactly the point of making it atomic?"** Atomicity protects the *value*, it guarantees
a read of `closed_` never sees a torn or partially-written bit pattern. It says nothing about
the *check-then-park* window inside a condition variable's wait: if `close()` stores the flag
without holding the mutex, a thread can evaluate its predicate as false, and, before it
finishes parking, `close()`'s store-and-`notify_all` can land in the gap, notifying an empty
waiting room. The thread then parks a moment later, having missed the only notification that
was ever going to come. The lock around the store is what closes that window; the atomicity
of the flag only matters for the *lock-free read path* that checks it without the mutex, not
for the write that actually has to coordinate with waiters.

**"You measured notify_all costing +9-15% over notify_one here, calling it 'modest', when
does that same cost stop being modest?"** When the number of waiters grows. This reading
explicitly hands off to [[025-thundering-herd]] for that case: with at most four threads in
a waiting room, waking all of them to let one or two actually proceed is a bounded, small
cost; scale that to hundreds of waiters on the same condition variable and the wasted-wakeup
cost, the exact thing 025's `wasted_wakeups` counter measures directly, grows with waiter
count while the useful work per notification doesn't.

**"A consumer's handler enqueues back into the same bounded queue, and the queue happens to
be full at that moment, what actually happens, and how would you prevent it at design
time?"** The consumer blocks inside its own call to `enqueue`, waiting for room, but it was
the only thread positioned to `dequeue` and make room in the first place. This is a deadlock
by construction, not by bad luck: any bounded queue with a cycle in its own dataflow graph
(a consumer that's also, transitively, a producer back into the same queue) deadlocks
whenever the queue fills, every time, deterministically. Preventing it at design time means
either restructuring so the dataflow graph has no cycles through this queue, or using
`try_enqueue` specifically on the back edge that closes the cycle, so it fails fast instead
of blocking forever.

**"Small machine, 2 cores, 4-8 threads on this queue. Does the single-mutex ceiling you
measured still apply, or does it only show up on wider machines?"** The reading's own numbers
describe the ceiling starting around 4-8 threads *total* on this 10-core machine, meaning
even a 2-core machine with that many logical threads competing for the one mutex hits the
same serialization ceiling, just reached via more time-slicing rather than more genuine
cache-line contention across physical cores. The mechanism differs slightly (OS scheduling
overhead vs cross-core cache-line bouncing) but the practical effect, one mutex caps total
throughput regardless of how many threads want in, holds on both.

**"Production ops, you close() a queue with producers still actively enqueueing. What's the
actual risk, and how would you monitor for it happening silently?"** `enqueue` returning
`false` after `close()` is the caller's only signal that an item was rejected, and most code
written against this API doesn't check that return value, meaning items can be silently
dropped during shutdown with zero diagnostic trail. Monitoring for it means tracking a
rejected-item counter explicitly incremented whenever `enqueue` returns `false`, surfaced as
a metric, its being nonzero after a shutdown is the concrete, checkable fact that data was
lost, which nothing in the class itself surfaces on its own.
