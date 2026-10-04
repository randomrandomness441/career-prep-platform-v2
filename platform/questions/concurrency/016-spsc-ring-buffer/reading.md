## 1. Reframe the problem

Every queue so far has used a mutex, and a mutex handoff costs microseconds. For an audio
callback that must produce a buffer every 5 ms, or a trading path measured in nanoseconds,
that is not a constant factor, it is a disqualification.

The way out is a restriction: **exactly one thread pushes, exactly one thread pops.**

That single constraint changes everything. `head_` now has exactly one writer (the
producer) and `tail_` has exactly one writer (the consumer). Neither index ever needs a
read-modify-write, so there is no compare-exchange, no retry loop, no lock. Both operations
complete in a bounded number of steps no matter what the other thread is doing, they are
**wait-free**, the strongest progress guarantee there is.

What remains is the only hard part: the two threads still have to agree on **what has been
published**. The producer writes an element and then advances an index; the consumer reads
the index and then reads the element. If those two pairs of operations can be observed out
of order, the consumer reads a slot it has been told is full and finds garbage.

So this question is not really about ring buffers. It is about **the minimum ordering you
must buy, and no more.**

## 2. The tools

### The layout

```cpp
alignas(64) std::atomic<std::size_t> head_{0}; // written only by the producer
alignas(64) std::atomic<std::size_t> tail_{0}; // written only by the consumer
std::array<T, Capacity> buf_{};
```

`head_ == tail_` means empty. One slot is always left unused so that "full" is
`(head_ + 1) == tail_` and can never be confused with empty.

### Power-of-two capacity

```cpp
static_assert((Capacity & (Capacity - 1)) == 0);
next = (h + 1) & kMask; // not (h + 1) % Capacity
```

A mask is one instruction. A general integer modulo is roughly 20 cycles. On a structure
whose whole purpose is a ~10 ns operation, that matters.

### The ordering, one decision at a time

**Producer reading its own index, relaxed.**
```cpp
const std::size_t h = head_.load(std::memory_order_relaxed);
```
We are the only writer of `head_`. Nobody else can have changed it. There is nothing to
synchronise with, so pay nothing.

**Producer reading the other index, acquire.**
```cpp
if (next == tail_.load(std::memory_order_acquire)) return false;
```
Pairs with the consumer's release store of `tail_`. We are about to overwrite `buf_[h]`,
and the consumer *read* `buf_[h]` before publishing this `tail_`. Acquire is what stops us
from scribbling over a slot the consumer has not finished reading.

**Producer publishing, release.**
```cpp
buf_[h] = v;
head_.store(next, std::memory_order_release);
```
This is the publication. Release says: everything written before this store, the element,
is visible to anyone who *acquires* this value of `head_`. This one line is the difference
between a correct queue and the naive version.

The consumer is the mirror image.

### What release/acquire actually buys

Think of it as a one-way barrier pair:

- **release** on a store: nothing written before it can be observed after it
- **acquire** on a load: nothing read after it can be observed before it

Together they form a **happens-before** edge from the producer's element write to the
consumer's element read, but only between the two threads that touch that pair, and only
for that pair. That locality is exactly why it is cheaper than `seq_cst`, which imposes a
single total order over *every* sequentially-consistent operation in the whole program.

## 3. The broken version, first

The naive version makes every index access relaxed. The reasoning is seductive and every
clause of it is true:

> "The indices are atomic so nothing tears. Each index has exactly one writer. The indices
> only move forward. Why pay for ordering I do not need?"

```cpp
buf_[h] = v;
head_.store(next, std::memory_order_relaxed); // <-- the bug
```

`relaxed` means **atomic, and nothing else.** It guarantees the store itself is indivisible
and it orders *nothing* around it. The compiler may move the `buf_[h] = v` write after the
index store. The CPU may make them visible to the other core in either order.

So the consumer can load a `head_` that says "slot 7 is full", read `buf_[7]`, and get
whatever was in that slot from a previous lap around the ring.

Two properties of this bug make it genuinely nasty:

**It does not reproduce reliably.** I measured the closely-related message-passing pattern
directly: with relaxed ordering, a consumer saw a stale payload **44 times in 30,000**
message passes, and in a separate experiment, only **1 run out of 8** of 20,000 trials
caught it at all. It is not rare enough to be safe and not common enough to be caught.

**ThreadSanitizer does not find it.** I verified this: the relaxed version of the same
message-passing structure produced *no TSan warning at all*. TSan detects **missing**
synchronisation, not **insufficient ordering**, it largely models atomics as ordered
regardless of the memory order you requested. The tool everyone reaches for is blind to
this entire bug class.

That combination, invisible to the sanitizer, invisible to testing, produces correct
output almost always, is why memory-ordering bugs reach production and stay there.

## 4. Real-world usage

**Why this structure exists.** The lock-free SPSC ring is one of the oldest concurrent data
structures, and it survives because it maps onto a hardware reality: a producer and a
consumer that can each own one end of a buffer need no arbitration at all. Lamport's 1977
proof of a lock-free single-producer/single-consumer queue predates C++'s memory model by
three decades; C++11 finally gave us the vocabulary to write it portably instead of with
inline assembly and platform fences.

**Where you meet it:**

- **Audio.** Every real-time audio callback is the consumer end of one of these. The
 callback thread must never block, never allocate, and never take a lock, a priority
 inversion there is an audible glitch. CoreAudio, JACK and ASIO all work this way.
- **Trading and low-latency networking.** Market-data feed handler on one core, strategy on
 another. LMAX's Disruptor is this idea industrialised.
- **Kernel and driver rings.** virtio, io_uring, NIC descriptor rings, a producer index and
 a consumer index in shared memory, with exactly these barriers.
- **Logging on a hot path.** The application thread pushes a pre-formatted record and never
 waits; a background thread drains to disk.

**Where NOT to use it:**

- **Never with more than one producer or one consumer.** The whole design rests on each
 index having exactly one writer. A second producer means two threads doing
 read-modify-write on `head_`, which this code does not do, and the structure corrupts
 silently. This is the most common way people misuse it.
- **Not when you need to block.** `push` returns `false` when full; it does not wait. If the
 caller's answer to a full queue is "sleep until there is room", you want the bounded
 blocking queue instead, and you have reinvented it badly if you spin here.
- **Not for large or non-trivially-copyable `T`.** The slots are constructed up front and
 assigned into. A `T` whose assignment allocates puts an allocator, and its lock, back on
 your hot path.
- **Not as a default.** A mutex queue is simpler, supports many producers, and is fast
 enough for the overwhelming majority of code. Reach for this when you have measured that
 the mutex is your bottleneck, not before.

## 5. Performance

Measured on this machine, 2,000,000 items through the ring, min of 9 repetitions:

| | ns per item |
|---|---|
| SPSC ring, mask wrap | **~11** |
| bounded blocking queue (mutex + 2 condvars), from the previous question | **~5200** |

That comparison is the reason the structure exists: roughly **450x** per item. The mutex
version pays a lock, a condition variable, and two context switches per element; this one
pays two atomic loads, a store, and an array write.

**A measurement I could not make honestly.** I tried to quantify what `alignas(64)` on
`head_` and `tail_` is worth, the theory being that if both indices share a 64-byte cache
line, every push invalidates the consumer's copy and every pop invalidates the producer's,
so the line ping-pongs between cores. That effect is real and well established.

I could not measure it stably here. The unpadded variant came out consistently around
**11.4 ns** across every run, while the padded variant swung between **10 ns and 115 ns**
depending on the run, with the minimum of nine repetitions still landing at 114 ns
sometimes and 10 ns other times. That is a tenfold swing in the *same binary* with the
*same input*.

The cause is almost certainly macOS scheduling threads across performance and efficiency
cores: two threads on two P-cores behave completely differently from one on a P-core and
one on an E-core, and requesting `QOS_CLASS_USER_INTERACTIVE` did not stabilise it. The
placement effect is an order of magnitude larger than the effect I was trying to measure,
so any number I quoted would be noise.

Two lessons, and the second is the useful one:

1. Keep the `alignas(64)`. The reasoning is sound, it costs 128 bytes, and no plausible
 mechanism makes it harmful.
2. **A benchmark that swings 10x between runs is not measuring what you think it is.** The
 discipline is to notice that and say so, rather than to run it once more until it prints
 a number that matches the story you wanted to tell. On heterogeneous CPUs, every Apple
 Silicon Mac, every modern phone, and increasingly Intel desktops, thread placement
 dominates micro-benchmarks, and any concurrency number you cannot reproduce across
 several runs is a number you have not actually measured.

## 6. Where this solution fails

- **Two producers or two consumers destroys it silently.** No assertion fires; the indices
 simply become wrong. If the restriction cannot be guaranteed by construction, document it
 loudly or use a different structure.
- **Capacity must be a power of two**, enforced by `static_assert` here. Without the mask
 you pay a division on every operation.
- **One slot is always wasted.** `Capacity` 1024 holds 1023 items. Alternatives that use
 every slot need a separate count or generation counter, which reintroduces a shared
 read-modify-write.
- **`push` failing is a design decision you must handle.** Returning `false` pushes the
 problem to the caller. Spinning until there is room converts a wait-free producer into a
 blocking one and can burn a core.
- **No cross-process safety as written.** Putting this in shared memory needs the indices to
 be lock-free *and* address-independent, and `std::atomic` gives you neither guarantee
 automatically. Check `is_always_lock_free`.
- **The element type must tolerate being overwritten in place.** Slots are default
 constructed at build time and reused every lap; a `T` with non-trivial destruction
 semantics needs a different design using placement new and explicit destruction.
- **It says nothing about *when* the consumer runs.** A wait-free queue with a consumer that
 never gets scheduled is a full queue. The structure removes the lock, not the need for the
 two threads to actually get CPU time.

## 7. Interview follow-ups

**"You measured ~450x over the mutex-based bounded queue. Where does that gap actually come
from, is it the lock, or something else?"** The mutex version pays a lock acquisition, a
condition-variable wait/notify round trip, and, under real contention, a context switch
per element; this design pays two atomic loads, one store, and an array write, with no
kernel involvement at all in the common case. The gap isn't "atomics are cheap and mutexes
are expensive" in the abstract (see [[030-atomics-vs-mutexes]] for a case where that
intuition reverses under contention), it's specifically that this structure never needs the
OS to arbitrate anything, because SPSC's single-producer/single-consumer restriction means
the two sides never actually contend for the same slot at the same instant, only for
*visibility* of the indices, which atomics handle without ever leaving user space.

**"You said you couldn't measure the alignas(64) padding's benefit stably, a 10x swing
between runs on identical input. Why report a negative result instead of just running it
until you got a clean number?"** Because a number obtained by re-running until it matches
the expected story isn't evidence, it's confirmation bias with extra steps. The swing itself
was diagnostic: it pointed at heterogeneous core placement (P-cores vs E-cores on this Apple
Silicon machine) dominating the measurement, an effect an order of magnitude larger than the
false-sharing effect being tested for, reporting that honestly is more useful than a fake
precise number, because it tells the next person measuring this exact thing what to control
for (core affinity) before trusting their own results.

**"Two producers are used against this SPSC design by mistake, what actually happens, and
how would you catch it before it ships?"** No assertion fires and no crash occurs
immediately, the single producer-side index (`head_`) becomes subject to the exact
lost-update race this course has shown repeatedly (see [[027-thread-local-storage]],
[[038-multithreaded-memory-pool]]) when touched by two threads with no atomicity between
them, since the design's whole safety argument assumes only one writer ever touches it. The
indices silently become wrong, items get overwritten before being consumed, or consumed
twice. Catching it before shipping means either a runtime check (an owning-thread-id
assertion in debug builds) or, better, a type-system-level restriction (a non-copyable
"producer handle" object that can only be constructed once) so misuse is a compile error
instead of a silent runtime corruption.

**"Extreme load, 10^8 items/sec through this queue, on a heterogeneous chip like this Apple
Silicon machine. What's the practical mitigation for the placement variance you measured?"**
Pin the producer and consumer threads to specific cores (ideally both performance cores, or
whatever core type the workload needs) via the platform's thread-affinity APIs, rather than
letting the OS scheduler place them freely, the reading's own measurement shows placement
variance can dwarf the actual algorithmic cost by an order of magnitude, so at extreme
throughput targets, controlling placement is a bigger lever than any further micro-
optimization of the ring buffer's own code.

**"This queue drops (or blocks) when full, how would you decide which, for a real system,
and what's the actual tradeoff?"** This is exactly [[024-backpressure]]'s question, applied
here: `push` returning `false` on a full queue pushes the decision to the caller (drop,
retry, or apply backpressure upstream), while spinning until space opens converts a
wait-free producer into a blocking one, which can burn a core doing nothing useful if the
consumer has stalled. The right choice depends on whether losing data is acceptable for this
specific stream (metrics: often yes; financial transactions: rarely), the structure itself
is agnostic and deliberately leaves that policy decision to the caller rather than making it
for them.
