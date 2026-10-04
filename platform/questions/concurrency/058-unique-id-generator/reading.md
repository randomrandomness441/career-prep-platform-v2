## 1. Reframe the problem

Forget the "ID generator" label for a second. This is the same problem as
[[015-treiber-stack]]. Some pieces of state have to change together, or not at all,
while threads are hammering on them, and the tool for that is the same: a
compare-and-swap loop that keeps retrying until it sticks.

In the Treiber stack, the state was one thing: the head pointer. Here it's two numbers
glued together, the current tick and how many IDs you've handed out in that tick. Those
two numbers can never disagree, the same way the stack's head pointer can never point at
a node someone else already popped.

So why does `worker_id` matter at all? Because there's no central coordinator. Nobody
checks IDs across machines before handing one out. The only way two independent
generators can guarantee they never collide is to make sure their output ranges never
overlap in the first place. Stamping a distinct `worker_id` onto every ID does exactly
that, for free, with zero communication between instances.

## 2. The tools, from scratch

**Pack the state that has to move together into one atomic.** Keep "current tick" and
"sequence" as two separate atomics, and "check the tick, then maybe reset the sequence"
becomes two operations. Two threads can slip in between them. Pack both numbers into a
single `std::atomic<uint64_t>`, tick in the high bits and sequence in the low bits. Now
one compare-and-swap changes both at once, or changes neither.

**The CAS retry loop:**
```cpp
uint64_t old_state, new_state;
do {
    old_state = packed_.load();
    // ... compute new_state from old_state ...
} while (!packed_.compare_exchange_weak(old_state, new_state));
```
Here's what `compare_exchange_weak` actually does. If `packed_` still equals
`old_state`, it swaps in `new_state` and returns true. If not, it writes the real
current value into `old_state` and returns false. When it fails, the loop just tries
again with fresh data. Nothing broke. No lock was ever held. The loser just redoes its
arithmetic and has another go.

**Bit-packing a fixed-width ID.** `(tick << 22) | (worker_id << 12) | sequence` works
because each field has a fixed width and they never overlap: 42 bits for tick, 10 for
worker, 12 for sequence, adding up to 64. This shape has a name: a Snowflake ID, after
Twitter's original 2010 design, copied everywhere since. Learn the name. If someone in a
system-design interview says "roughly-sortable, coordination-free, distributed unique
ID," this is exactly what they mean.

## 3. The broken version, first

The naive version keeps `last_tick_` and `seq_` as two separate atomics:

```cpp
if (now > prev) {
    last_tick_.store(now);
    seq_.store(0);
    seq = 0;
} else {
    seq = seq_.fetch_add(1) + 1;
}
```

Each `.store()` and `.fetch_add()` on its own is atomic, sure. But the real question,
"am I the thread that gets to reset the sequence for this tick," is not one atomic
operation. Nothing stops two threads from both seeing `now > prev` before either one has
actually stored anything. Line up 16 threads to race into exactly that moment, no
artificial delay needed, and at `-O2` this is what happens:

```
trial 0: 16 threads raced one tick transition -> 14 distinct ids (want 16)
trial 1: 16 threads raced one tick transition -> 15 distinct ids (want 16)
trial 2: 16 threads raced one tick transition -> 12 distinct ids (want 16)
trial 3: 16 threads raced one tick transition -> 13 distinct ids (want 16)
trial 4: 16 threads raced one tick transition -> 14 distinct ids (want 16)
```

2 to 4 duplicate IDs out of 16, every single trial. Several threads all see "new tick"
at once. Each one independently computes `sequence = 0`. Each one hands out an ID that
some other thread already handed out. No error. No crash. Nothing in the return value
warns you. In a real ID generator, this shows up later as corrupted primary keys or
duplicate request IDs, far from wherever the actual bug lives.

There's a second bug too, quieter but just as real: masking `seq` back into range once
it overflows 4095. Once more than 4096 IDs get requested inside one tick, the naive
version starts reusing sequence numbers 0, 1, 2, the ones it already handed out earlier
in that same tick. Same result, exact duplicates, except this time on a predictable
schedule instead of a racy one.

## 4. Real-world usage

This exact idea shows up everywhere: Twitter's Snowflake, Discord's snowflake variant,
Instagram's sharded IDs, Sony's Sonyflake. Same idea each time, just different bit
widths. Encode time, a machine or shard ID, and a per-tick counter into one integer, and
any number of independent generators on any number of machines, with zero communication
between them, can never collide. This is the standard answer to "how do you generate
unique IDs across a distributed system without a single point of contention," and that's
exactly the shape of this question too.

It's the wrong tool in two cases though. First, if you need IDs to be strictly
sequential with no gaps, like an invoice number. Sequence resets every tick and jumps
forward on overflow, so gaps are expected here, and that would break a "no gaps, ever"
requirement somewhere else. Second, if the ID has to reveal nothing about when it was
created. The tick sits right there in the high bits, plain to read for anyone who gets
the ID.

## 5. Performance guarantees

**The CAS version is faster with one thread, and gets dramatically slower than a plain
mutex once several threads are actually fighting over it. That's the opposite of what
"lock-free is obviously faster" would have you guess.** Here's a direct measurement, one
generator, clock held fixed so we're isolating retry behavior from clock noise:

```
1 thread:  CAS  6.0 ns/id   mutex 10.0 ns/id
2 threads: CAS  8.5 ns/id   mutex 13.9 ns/id
4 threads: CAS 29.1 ns/id   mutex 16.4 ns/id
8 threads: CAS 278.7 ns/id  mutex 20.4 ns/id
```

At 1 and 2 threads, the CAS loop wins outright. No kernel involved, no wait queue. By 4
threads it's already lost to the mutex. By 8 threads it's roughly **14x slower**. Here's
why: every thread that loses its CAS has to reload the atomic, recompute the new state,
and try again. Under heavy contention on one cache line, most threads lose most of the
time. So the total work across all threads grows faster than the actual useful work of
producing IDs. A mutex does something different: it lets losing threads sleep instead of
burning cycles on doomed retries.

This course has now measured the same crossover three times: the Treiber stack, the
memory pool, and here. That's not a coincidence, it's a pattern worth remembering. **A
CAS retry loop is a bet that contention stays low. Lose that bet, and it doesn't degrade
gracefully. It just loses.** Whether that matters for a real ID generator comes down to
one question: how many threads actually call `next_id()` at once in the real system?
Measure it. Don't guess.

## 6. Where this solution fails

- **Clock going backward.** Say the clock, the injected one or the real one in
  production, reports a value smaller than a tick this generator already used. That
  happens for real: NTP can step the wall clock backward, the same lesson from
  [[009-timeouts]] about `system_clock`. When it happens, `now > old_tick` is false, so
  the generator just keeps using its own advanced tick instead of regressing. No
  duplicate ID, no decreasing ID. But it does silently drift ahead of real time until the
  clock catches up, and a monitoring system watching tick vs wall clock should expect
  exactly that.
- **Very high sustained overflow.** If a generator permanently produces more than 4096
  IDs per tick, its internal tick keeps running further and further ahead of the real
  clock. Brief bursts are fine. A sustained rate above 4096 per tick, forever, is not
  what this design was built to absorb.
- **The contention crossover from section 5.** At sustained high thread counts on one
  generator instance, the CAS loop's own cost becomes the bottleneck. That's why real
  deployments of this pattern usually shard by more than just `worker_id`, often one
  generator per CPU core, just to keep per-generator contention low enough that the CAS
  loop stays on its winning side.

## 7. Interview follow-ups

**"Why pack tick and sequence into one atomic instead of two?"** Because "check the
tick, then maybe reset the sequence" has to be one indivisible step, not two. See
section 3 for what goes wrong otherwise. Whenever two pieces of state must never be seen
in an inconsistent combination, and you want to avoid a lock, packing them into one word
that a single CAS can change is the standard move.

**"What happens if this generator runs on a machine where `std::atomic<uint64_t>` isn't
lock-free?"** Call `packed_.is_lock_free()` and check, before you assume the "no mutex"
design is actually paying off. On some 32-bit or embedded targets, a 64-bit atomic is
implemented with a hidden lock inside the standard library. If that's true here, you've
built a mutex shaped like a CAS loop, and none of the numbers in section 5 apply to you.

**"How would you extend this to guarantee IDs are strictly increasing even across
different worker_ids, not just within one generator?"** You can't, not without giving up
the one property this whole design exists for: no coordination between instances.
Strict global ordering across independent generators needs either a tightly
synchronized shared clock, which is still only approximate, or a single coordinating
sequencer, which brings back the exact bottleneck this design was built to avoid.
That's a real trade-off in every distributed ID scheme. It's not a gap in this one.
