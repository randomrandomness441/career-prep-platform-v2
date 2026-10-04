## 1. Reframe the problem

`std::atomic<int> counter{0}; counter++;` and `{ std::lock_guard<std::mutex> lk(m); ++counter; }`
both make the increment safe. The question worth asking isn't "which one is safe", both
are, it's "what am I actually protecting, and does that thing need a mutex's full
machinery?"

A mutex protects a **critical section**, an arbitrary sequence of operations that has to
happen as one atomic-looking unit from every other thread's point of view. It can guard ten
lines that touch three data structures. An atomic protects **one single memory location**
and nothing else, the hardware itself guarantees the read-modify-write happens as one
indivisible step, with no OS involved at all. When your entire "critical section" is one
word getting incremented, a mutex is buying you machinery (a kernel-arbitrated lock, a
possible thread sleep, a possible context switch) to protect something the CPU can already
do atomically in hardware, for free. That mismatch, reaching for the general tool when the
specific one fits exactly, is the whole lesson.

## 3. The broken version, first

```cpp
class HotCounter {
    long counter_ = 0;
public:
    void increment() noexcept { ++counter_; } // <-- the bug
    long get() const noexcept { return counter_; }
};
```

**Why it looks right, and who it fools:** single-threaded, this is exactly correct, and
`++counter_` reads like one operation because it's one line of C++. It's easy to forget that
"one line of C++" and "one indivisible step" are unrelated properties, `++counter_` is a
load, an add, and a store, three separate memory operations with no relationship enforced
between them and whatever another thread is doing to the same variable at the same moment.

Run it for real, 8 threads, each calling `increment()` 200,000 times on one shared
`HotCounter`, no lock, no atomic:

```
8 threads x 200000 increments: got 403531, expected 1600000 (1196469 lost)
```

Almost 75% of the increments vanished. Two threads each read the same value, each added
one, each wrote back the same result, one increment overwrote the other and disappeared,
over and over, 1.2 million times in this one run. **This is not a rare, hard-to-hit
interleaving**, it happened on every single run this question's test made, because at any
real level of contention the collision window opens constantly, not occasionally.

One implementation note worth knowing, because it will bite you if you try to "prove" this
race with your own quick test: if the number of increments is a compile-time constant, an
optimizing compiler is *allowed* to notice the loop is undefined behaviour under a race and
collapse the whole per-thread loop into a single `counter_ += 200000` at the very end, one
read-modify-write per thread instead of 200,000, which makes the bug nearly disappear by
accident. This course's own test had to defeat that by making the loop bound `volatile`
(forcing a real memory read every iteration) to get the race to show up reliably. The lesson
underneath the lesson: **undefined behaviour is not guaranteed to misbehave, and "my test
passed" is not proof a racy program is fine**, it may just mean the optimizer hid the
symptom this time, on this compiler, at this optimization level.

The fix is the one-line change from the hints, swap the storage for `std::atomic<long>`
and the increment for `fetch_add`:

```cpp
counter_.fetch_add(1, std::memory_order_relaxed);
```

Same 8 threads, same 200,000 increments each:

```
8 threads x 200000 increments each: exact total, no updates lost
```

Every time. The hardware's atomic instruction (`LDADD`/`LSE` on Apple Silicon, `LOCK XADD`
on x86) makes the whole read-modify-write indivisible at the instruction level, there is no
window between the read and the write for another core to land in, so there is nothing to
interleave.

**Why this is the answer at senior level, and why it isn't always.** The guide's own claim
here is worth checking rather than trusting: it says an uncontended mutex costs "1000+
cycles" against an atomic's ~20. Measured directly on this machine (Apple Silicon,
single thread, no contention, 20,000,000 iterations, `-O2`):

```
uncontended mutex : 6.8 ns/op
uncontended atomic: 3.2 ns/op
```

Roughly **2x**, not 500x, a real difference, but nowhere near "1000+ cycles"; even at a
generous 4 GHz that would be 250 ns, thirty-seven times what was actually measured. macOS's
`std::mutex` (built on `os_unfair_lock`) is already fast in the fully uncontended case,
there's no kernel round-trip when nobody's waiting. The 1000-cycle number is a real cost,
but it belongs to the *contended*, thread-goes-to-sleep-and-wakes-up path, not the baseline
lock/unlock, the guide's mistake is quoting the worst case as if it were the typical one.

The more interesting number is what happens under real contention, see section 6.

## 6. Where this solution fails

- **The "one variable, one operation" condition is easy to lose without noticing.** The
 moment `HotCounter` needs a second field that has to stay consistent with the first (a
 running sum *and* a count, a value *and* a timestamp of last update), `fetch_add` on one
 of them no longer protects the pair, you need a mutex, or two atomics with a documented
 (and easy to get wrong) protocol between them, or a single atomic holding both packed into
 one word.
- **Under high contention, the atomic can lose to the mutex, measured, not theoretical.**
 8 threads hammering the *same* shared counter, 2,000,000 increments each, this machine:

 ```
 contended(8) mutex : 20.3 ns/op (median across 4 runs)
 contended(8) atomic : ~32 ns/op (median across 4 runs, range 29-36)
 ```

 The atomic was consistently **~1.5x slower** than the mutex here, every run. The reason is
 exactly the guide's follow-up #2, now with numbers behind it: eight cores doing
 uncoordinated `fetch_add` on one cache line produce a continuous storm of cache-line
 invalidation (MESI ping-pong) with no backoff at all, every core is always retrying.
 A mutex, when contended, can let the losing threads actually sleep instead of spinning,
 which cuts down how much bus traffic the "losers" generate while they wait. This is the
 qualifier the guide's Q1 leaves out: atomics win by a wide, reliable margin
 *uncontended*, and can lose under *extreme* contention on the same line, which of those
 two regimes you're in is an empirical question, not something you get to assume.
- **`memory_order_relaxed` only orders the counter itself.** It guarantees the increment is
 atomic and that every thread eventually agrees on the final value; it says nothing about
 the visibility of any *other* variable relative to this one. If `increment()` is meant to
 signal "the data I just wrote is ready," relaxed is the wrong order, that's a
 release/acquire handoff (see the SPSC ring buffer and store-buffering questions), not a
 plain counter.
- **Padding matters here too, and this exercise doesn't test for it.** Put two independent
 `HotCounter`s next to each other in memory and you've reintroduced [[029-false-sharing]]
 even though each one is individually correct and lock-free.
- **Not every type can be atomic.** `std::atomic<T>` needs `T` to be trivially copyable, and
 lock-free atomics in practice top out around the machine's native word size (8-16 bytes,
 sometimes 16 with `DCAS`/`cmpxchg16b`). A struct with three `long`s has no atomic
 fast path at all, `std::atomic<BigStruct>` compiles but silently falls back to an
 internal mutex, which is worth checking with `.is_lock_free()` rather than assuming.

## 7. Interview follow-ups

**Q: Why is `std::atomic<int>::operator++` faster than a mutex-guarded increment?**
A: It's one hardware instruction with no OS involvement, no syscall, no possibility of the
thread sleeping, no context switch. A mutex *can* be just as cheap when uncontended (this
machine measured ~6.8 ns for lock+unlock against ~3.2 ns for the atomic, about 2x, not the
often-quoted 1000x), but it's carrying machinery, kernel arbitration, a wait queue, that
the atomic doesn't need to pay for at all in the no-contention case.

**Q: When does an atomic become slower than a mutex?**
A: Under heavy contention on the *same* location. This course measured it directly: 8
threads on one shared counter, the atomic ran ~1.5x slower than the mutex, consistently,
because uncoordinated hardware retries produce continuous cache-line invalidation traffic
with no backoff, while a contended mutex can let losing threads sleep instead of spin.

**Q: What is `std::memory_order_relaxed`, and when is it enough?**
A: It guarantees the operation itself is atomic (no torn reads, a single global order of
modifications to *that* variable) but imposes no ordering on anything else in the program.
It's enough exactly when nothing else needs to be made visible alongside this value, a
pure counter, a statistics tally. The moment the counter's new value is supposed to signal
"some other data is now ready to read," you need release/acquire instead.

**Q: Small machine, 1-2 cores. Does the mutex-vs-atomic gap matter?**
A: Barely. With effectively no real concurrency, "contention" mostly doesn't happen, one
thread runs, then the other, and both a mutex and an atomic cost close to their uncontended
price. The interesting numbers in this reading only show up because 8 real cores can
genuinely collide at the same instant.

**Q: Big machine, 64+ cores, NUMA, all hammering one shared atomic. What happens?**
A: It gets worse than this question's 8-core measurement, not better. More cores means more
simultaneous retries on the same cache line, and on NUMA hardware the line may have to
travel across sockets on every bounce, the mutex's "let the loser sleep" advantage tends to
widen, not shrink, as core count grows. Past a certain thread count, neither a plain shared
atomic nor a plain shared mutex scales, the real fix is sharding the counter per thread (or
per core) and summing on read, which is a different data structure, not a different lock.

**Q: A profiler shows a hot atomic increment. How do you tell if it's actually the
bottleneck, or something else?**
A: Check whether the time is in the instruction itself (uncontended cost, ~3 ns here, never
your bottleneck) or in retries/cache misses around it (the contended cost, which scales with
however many cores are hitting the same line). `perf stat -e cache-misses` or, on Apple
platforms, Instruments' "Cache Misses" / "System Trace" template will show whether the
counter's cache line is bouncing. If it is, the fix is almost never "use a different
primitive", it's reducing how many cores touch that one line, usually by sharding it.

**Q (reported Pure Storage question): 5 threads each run `for (i=0;i<5;i++) global++;` on a
plain `int global = 0`, no synchronization at all. What are the minimum and maximum possible
final values of `global`, and why?**
A: **Maximum is 25** and needs no subtlety: if the 25 increments never actually overlap in
time (the OS just happens to run them one after another), every single one lands and you get
the fully serial answer.

**Minimum is 2, not 1 and not 0**, the number that trips people up. `global++` is not one
hardware step; it's read, add one, write back. The worst case for lost updates is *every*
thread reading the same stale value and writing back the same result, but that requires
every one of the 25 increments to read `global` at a moment when no write has landed yet,
which means at most one write can ever actually land, because the instant any thread's write
does land, every *subsequent* read (even a stale-relative-to-some-other-thread one) sees a
value ≥ 1, and at least one more increment operating on that already-changed value is
guaranteed to eventually complete and store something new. Formally: consider the *last*
write to complete in real time, whatever value it stores must be at least 1 greater than
whatever it read, and it's writing last, so that value sticks. That accounts for one
completed increment surviving. Now consider that among the 5 threads, thread scheduling
means at least one thread's *very first* read happens before any thread has written anything
, that read-add-write triple, if it's the only one that's not clobbered, is a second surviving
increment independent of the "last write" one. Getting exactly down to 2 requires an
adversarial interleaving where every other read-modify-write in the program is a lost
update, every one of them reads a value some other thread is about to overwrite before it
gets to write its own result back. It's a genuinely fiddly interleaving to construct by hand,
which is exactly why the interviewer expects you to reason about it structurally (as above)
rather than trying to enumerate schedules.

*A wrinkle worth naming if the interviewer pushes further:* this reasoning already assumes
every thread actually gets scheduled and every increment source line runs, which the C++
standard does not technically guarantee for a *data race* (unsynchronized concurrent
read/write of a non-atomic variable is undefined behavior, full stop; a real compiler is
permitted to do things far stranger than "some updates are lost," including hoisting the
load out of the loop entirely under `-O2`). The min-is-2 argument is the right answer for "if
this compiles to what it looks like it compiles to, on real hardware, with no optimizer
tricks", which is what the interviewer is actually asking about (the mechanics of
read-modify-write races), but it's worth flagging that the standard-legal answer to "what's
guaranteed" is "nothing, it's UB" before diving into the hardware-level reasoning.
