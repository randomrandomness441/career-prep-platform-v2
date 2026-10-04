## 1. Reframe the problem

Every other question in this course fixed races by adding synchronization, a mutex, a
condition variable, an atomic flag. This one is different: **both operations here are
already atomic.** There is no data race by the language's own definition, and
ThreadSanitizer will confirm that on every version you write, correct or not. So what's
actually broken?

The bug isn't about *whether* an operation is atomic, it's about *what other threads are
allowed to see, and in what order,* around that atomic operation. `std::memory_order` is a
second, independent knob from atomicity itself: atomicity says "this read or write cannot be
torn"; memory order says "how much freedom does the compiler and the CPU have to reorder this
operation relative to other memory operations, on this thread and as observed by others."
Get the first one right and the second one wrong, and you get exactly this question: no
crash, no TSan warning, a data structure that's individually thread-safe at every access,
and an invariant that's still false, occasionally, for reasons that only show up under real
concurrent hardware.

## 2. The tools

### The six memory orders, and the two you need here

`std::memory_order_relaxed` guarantees atomicity and nothing else, no ordering constraint
relative to any other memory access, on any thread. `std::memory_order_seq_cst` (the
default if you name no order, `a.store(1)` is `a.store(1, std::memory_order_seq_cst)`) is
the strongest: every seq_cst operation across every thread in the program is placed into one
single, total order that all threads agree on. Between those two extremes sit
`acquire`/`release`/`acq_rel`/`consume`, which buy a narrower guarantee: a `release` store
and an `acquire` load *of that same atomic variable, by the same thread that reads the value
the release wrote* form a **synchronizes-with** pair, everything the releasing thread did
before its store becomes visible to the acquiring thread after its load. That's a pairwise
promise between one writer and one specific reader of the value it wrote, not a global-order
promise.

### Why "atomic" doesn't mean "the other thread sees my write instantly"

Real CPUs buffer stores. A core that writes to memory doesn't necessarily push that write out
to where other cores can see it right away, it can sit in a per-core store buffer while the
core moves on to its next instruction, including a *read* of some other address. If both
cores do this at once, write my flag, read yours, it is physically possible for each core
to read the other's variable *before* either write has left its own store buffer. Both reads
see the old value. This is called **store buffering**, and it's not a bug in the hardware,
it's a deliberate performance optimization that every mainstream CPU makes, and the memory
order is the only thing that tells the CPU when it's not allowed to do this.

### Reading the litmus-test shape

This exact two-write-then-two-read shape, thread 1: `a = 1; r1 = b;`, thread 2: `b = 1; r2 =
a;`, is a standard tool in memory-model literature, usually called "SB" (store buffering).
It's small on purpose: small enough that "can `r1 == 0 && r2 == 0` happen?" has a clean yes
or no answer for each memory order, and the answer is genuinely surprising the first time you
see it, which is the whole point of studying it.

## 3. The broken versions, first

### Attempt one: relaxed

```cpp
a_.store(1, std::memory_order_relaxed);
return b_.load(std::memory_order_relaxed);
```

**Why it looks right:** both operations are on `std::atomic`, which is "the thread-safe
type", it's easy to read "atomic" as "safe" and stop there, especially since there's no
mutex to reason about and no TSan warning to second-guess. 20,000 trials, fresh threads each
time:

```
invariant broken 16/20000 trials
```

Small, sixteen out of twenty thousand, and that's exactly what makes it dangerous: it will
pass a quick manual test, it will pass most code review, and it will show up in production
as an intermittent, unreproducible-looking bug months later. Re-run the same 20,000 trials
across twelve separate executions and it never once comes back at zero, the reordering is
real and repeatable, just rare per individual trial.

### Attempt two: release/acquire, better, and still not enough

The instinct once you know relaxed is too weak: "make the write a release and the read an
acquire, that's the standard fix for visibility." Try it:

```cpp
a_.store(1, std::memory_order_release);
return b_.load(std::memory_order_acquire);
```

Ten repeats of 20,000 trials each:

```
0, 0, 0, 0, 1, 0, 1, 0, 0, 0 (violations per run)
```

Mostly zero, noticeably better than relaxed's every-run failures, but not reliably zero.
Two runs out of ten still broke the invariant. **This is the important result of this
question:** release/acquire did *not* fix this pattern, even though release/acquire is the
textbook fix for "make my write visible to the reader." The reason is in section 1's
definition, release/acquire synchronizes one store with the one load *that reads the value
it wrote*. Here, thread A's read (of `b_`) has no release/acquire relationship with thread
A's own write (to `a_`) at all, they're different variables, connected only by being in the
same thread, and ordinary program order within a thread says nothing about what a *different*
thread sees. The synchronizes-with edge this pattern needs doesn't exist in a release/acquire
design, no matter how you assign the orders to these four operations.

### The fix: seq_cst

```cpp
a_.store(1, std::memory_order_seq_cst);
return b_.load(std::memory_order_seq_cst);
```

Twelve repeats of 20,000 trials, plus this question's own full verification (12 correctness
runs, 20 TSan runs, 150 shaken-stress runs):

```
violations=0/20000 (x12, and 0 again across every stage of full verification)
```

seq_cst's single-global-order guarantee is exactly what this pattern needs: A's store and B's
store land in *some* order in that one timeline every thread agrees on. Whichever one is
second, the *other* side's read, also seq_cst, is guaranteed to be positioned after it in
that same global order, and therefore sees it.

## 4. Real-world usage

**Why memory orders weaker than seq_cst were invented at all.** Sequential consistency is the
easiest model to reason about, it's "as if" all threads' operations were interleaved on one
timeline, exactly the mental model most programmers already have. It's also the most
expensive to implement on real hardware, because it forbids optimizations (like store
buffering) that every modern CPU relies on for performance. C++11's memory model offers the
weaker orders specifically so code that doesn't need the full guarantee doesn't have to pay
for it, a plain reference counter's decrement only needs `relaxed` (nobody needs to see
*when* it happened, only the final atomic value), while releasing a lock needs
`release`/`acquire` (the next lock-holder must see everything the previous one wrote).

**Where you meet this pattern in production:**

- **Peterson's and Dekker's mutual-exclusion algorithms** are built directly on this shape,
 each thread announces intent, then checks the other's, and both are textbook-documented
 as broken under relaxed memory models for exactly this reason, which is why real
 implementations either use seq_cst or an explicit fence.
- **Any "did the other side also start" check built from two independent flags**, a
 double-checked handshake, a lock-free two-party rendezvous, has this shape hiding inside
 it, whether or not the author recognizes it as the SB litmus pattern.
- **Sequentially consistent atomics are the default for a reason.** `std::atomic`'s default
 memory order is seq_cst specifically so that reaching for a weaker order is an active,
 visible choice, not something you fall into by omission.

**Where NOT to reach for seq_cst:** most atomic operations in real code are single-variable
counters, flags, or reference counts with no second variable's ordering to coordinate against
, a plain increment (`relaxed`), or a straightforward producer/consumer handoff of one piece
of data behind one flag (`release`/`acquire`, which *is* sufficient there, see
[[016-spsc-ring-buffer]], where the producer's `release` store of `head_` and the consumer's
`acquire` load of it form exactly the synchronizes-with pair this pattern is missing).
Defaulting every atomic to seq_cst "to be safe" costs real
performance at scale (see section 5) for a guarantee most individual operations don't need;
reserve it for patterns that specifically require a shared global order, like this one.

## 5. Performance

Single-threaded, uncontended, this machine (Apple Silicon, ARM64), 200,000,000 iterations,
each store/load forced to actually execute via a compiler memory-clobber barrier so the
loop can't be optimized into nothing:

```
relaxed store: 0.46 ns/op seq_cst store: 0.46 ns/op (no measurable difference)
relaxed load: 0.47 ns/op seq_cst load: 0.45 ns/op (no measurable difference)
```

That's a genuinely useful negative result, not a rounding error being ignored: on this ARM64
implementation, an *uncontended, single-threaded* seq_cst operation costs about the same as a
relaxed one. The instructions differ (seq_cst compiles to `STLR`/`LDAR`, ARM's
store-release/load-acquire forms, versus plain `STR`/`LDR` for relaxed), but that difference
doesn't show up as measurable latency in isolation on this hardware. **The real cost of
seq_cst is not per-operation latency, it's the reordering freedom it takes away from the
compiler and from cross-core traffic under actual contention**, which a single-threaded
loop with nothing else happening cannot exercise. This is exactly why "just use seq_cst
everywhere, it's basically free" is a trap: it's cheap in a microbenchmark and expensive in a
program where many cores are actually contending over the same cache lines, which single-
threaded measurement structurally cannot show you.

## 6. Where this solution fails

- **This is architecture-specific in ways that will bite a relaxed version differently on
 different hardware.** The relaxed violation rate measured here (roughly 1 in 1,300 trials)
 is a property of *this* CPU's store-buffer depth and timing, not a guarantee. On a
 different microarchitecture, or even a different core type on the same chip, under
 thermal throttling, under different background load, the rate could be much higher or
 (misleadingly) much lower. "I tested it and it seemed fine" proves nothing here; only the
 standard's guarantee does.
- **Compilers are also allowed to reorder relaxed operations, not just hardware.** Everything
 measured here is a hardware effect on ARM64; on a different compiler, or with more
 surrounding code for the optimizer to work with, a relaxed operation could be reordered at
 compile time too, on hardware that wouldn't have reordered it itself. Treat the memory
 order as the actual contract, not the specific violation rate you happened to observe.
- **seq_cst doesn't compose the way people assume.** It guarantees a single total order over
 seq_cst *operations*, it says nothing extra about non-atomic or relaxed accesses that
 happen to sit near them in the code, unless those accesses are themselves ordered relative
 to a seq_cst operation through release/acquire's usual rules (a seq_cst store is also a
 release; a seq_cst load is also an acquire).
- **This litmus test proves the invariant holds for *this specific* two-variable, two-thread
 shape.** It doesn't generalize automatically to three threads checking each other, or to
 more than one variable per side, those need their own reasoning (and often their own
 litmus test), not an assumption that "seq_cst fixes memory ordering" as a blanket rule.

## 7. Interview follow-ups

**"You said release/acquire didn't fix this. When *does* release/acquire fully solve a
two-thread problem?"** When there's a genuine one-directional handoff: thread A writes some
data, then release-stores a flag; thread B acquire-loads that same flag, and only proceeds to
read A's data *after* seeing it. The synchronizes-with edge is between A's flag-store and B's
flag-load, exactly the pair that matters for "did B see A's data." This litmus test has no
such single load-of-the-store-that-matters pairing; A's read targets a *different* variable
than the one it wrote, so there's no release/acquire pair to lean on. See
[[016-spsc-ring-buffer]] for the case where release/acquire is exactly the right and complete
fix, one thread publishes, the other consumes, and there's a single store/load pair that
actually is the handoff.

**"Small machine, a single core, everything time-sliced.** Does this bug exist there?" No,
store buffering is a *cross-core* hardware effect; a single core executing one thread at a
time (even with context switches) retires its own stores in program order as far as that core
can tell, and a context switch is a full pipeline flush, which is stronger than any of these
memory orders individually. The bug requires two things actually running on two separate
cores at the same moment.

**"Big machine, many-core, NUMA. What changes?"** The store-buffer effect measured here gets
*more* likely, not less, as memory hierarchies get deeper, a write on a NUMA machine may sit
in a local buffer even longer before becoming visible to a core on a different socket, and
the interconnect between sockets adds its own latency to when a seq_cst fence's effects
actually propagate. The correctness story doesn't change (seq_cst still guarantees the single
global order), but the seq_cst fence's actual cost goes up with NUMA distance in a way the
single-socket measurement in section 5 can't show.

**"How would you even find this class of bug in a large, unfamiliar codebase, you can't run
20,000 trials on every atomic access."** You largely can't find it by testing, the whole
lesson of this question is that TSan and a reasonable stress test both come back clean on the
broken version. What actually works: recognize the *shape* (any place two threads each write
one variable and read the other's) and check the memory orders used against what the pattern
actually requires, the way you'd check any other proof obligation, this is one of the few
areas of concurrency where static reasoning about the standard's guarantees is more reliable
than dynamic testing.

**"Production ops: this bug shipped anyway. What does it look like in the wild?"** An
intermittent, low-frequency correctness failure with no crash and no error log, exactly the
kind of bug that gets closed as "couldn't reproduce" after a cursory look, then resurfaces
weeks later under different load. The only reliable signal is a *rate*, not a reproduction
case: if you can instrument the specific invariant being violated (as this question's own
`wasted`-style counters do for other questions), tracking its violation rate over time is more
useful than trying to catch one instance in a debugger.
