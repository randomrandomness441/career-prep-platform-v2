## 1. Reframe the problem

A mutex-protected stack works by making everyone else stop. A lock-free stack works by
making every change to shared state a **single indivisible instant**, one atomic
read-modify-write that either lands whole or does not land at all, and then retrying when
it does not land.

For a stack that is easy, because there is exactly one word that defines the whole
structure: the head pointer. Everything else in a node is written before the node is
visible to anybody, or read after the node is exclusively yours. So push and pop are both
the same shape: *read the head, compute what the head should become, swap it in if nobody
beat me, otherwise start over.*

That part takes about ten lines and is not the hard part.

The hard part is the question a mutex answers for free: **when is it safe to `delete` a
node?** Under a mutex, the moment you hold the lock you know no one else is inside the
structure. Without a lock, a thread that popped a node has no idea how many other threads
are, right now, holding a pointer to that same node and about to dereference it. Freeing it
is a use-after-free; reusing its address is the ABA problem. Every serious lock-free data
structure spends most of its complexity here, and none of that complexity is about push or
pop.

So the real question is not "how do I write a lock-free stack". It is: **how do I retire
memory that other threads might still be reading?**

## 2. The tools, from scratch

### `std::atomic<T*>` and compare-exchange

An atomic pointer is a pointer that several threads may read and write at once without it
being a data race, and without the value ever being half-written.

```cpp
std::atomic<Node*> head{nullptr};
```

The one operation that makes lock-free programming possible is compare-exchange:

```cpp
bool ok = head.compare_exchange_weak(expected, desired);
```

Read it as: *"if `head` is still `expected`, make it `desired` and return true. If it is
not, do not change it, instead write the value it actually holds into `expected`, and
return false."*

That second clause is the useful part and the part people miss. On failure the function
**overwrites your `expected` variable** with the current value, so a retry loop needs no
body:

```cpp
long cur = counter.load(std::memory_order_relaxed);
while (!counter.compare_exchange_weak(cur, cur + 1)) {
    // empty, cur has already been refreshed for us
}
```

The whole compare-and-swap is one indivisible step. No other thread can slip a write in
between the compare and the swap; that is the entire guarantee, and everything below is
built out of it.

### `_weak` versus `_strong`, and what this machine actually does

`compare_exchange_strong` fails only if the value genuinely differed.
`compare_exchange_weak` may *also* fail spuriously, it can report failure even when the
value matched.

Why would a library offer something that lies? Because on load-linked/store-conditional
machines, classic ARM, POWER, RISC-V, there is no single compare-and-swap instruction.
The CPU offers a pair: `ldxr` ("load exclusive, and remember I am watching this address")
and `stxr` ("store, but only if nothing touched that address since"). The store can fail
for reasons that have nothing to do with your value: an interrupt, a context switch,
another core touching an unrelated variable in the same cache line. `_weak` maps directly
onto that pair and reports the failure. `_strong` has to *hide* it, which means wrapping
the pair in an extra loop.

Compiled for baseline ARMv8-A, that is exactly what you get:

```
weak_one: strong_one:
 ldxr x8, [x0] .LBB1_1:
 b.ne .LBB0_2 ldxr x8, [x0]
 stxr w9, x2, [x0] b.ne .LBB1_4
 ... stxr w10, x2, [x0]
 cbnz w10, .LBB1_1 <-- the extra retry loop
```

Inside a retry loop you do not care *why* the exchange failed, you were going to loop
anyway, so paying for that inner loop buys nothing. Hence the rule: **`_weak` inside a
loop, `_strong` when a single attempt has to be conclusive.**

Now the honest part, measured on this machine. Apple Silicon implements ARMv8.1-A LSE
atomics, which include a real single-instruction compare-and-swap. Compile the same two
functions for this target and:

```
_weak_one: _strong_one:
 cas x8, x2, [x0] cas x8, x2, [x0]
```

**Identical.** One instruction each. There is no spurious failure to hide, so `_strong`
costs nothing extra here, and timing the two in a contended loop produces differences that
move around between runs, noise, not signal. Use `_weak` anyway: it is free on this
machine and it is the difference between 1 and 2 instructions per attempt on the ARM chips
your code will also run on.

### push

```cpp
void push(T value) {
    Node* n = new Node(std::move(value));
    n->next = head_.load(std::memory_order_relaxed);
    while (!head_.compare_exchange_weak(n->next, n,
    std::memory_order_release,
    std::memory_order_relaxed)) {
    }
}
```

Three things are going on.

- **`n` is private until the CAS succeeds.** No other thread can reach it, so filling in
 `data` and `next` needs no synchronisation at all. Publication is the CAS.
- **`n->next` is the `expected` slot.** That is not a trick for its own sake: on failure the
 compare-exchange writes the new head into `n->next`, which is precisely the value the next
 attempt needs. The loop body is empty because the retry has already been set up.
- **`release` on success.** This is the one memory-order decision. It says: everything this
 thread wrote before the CAS, the node's value, its next pointer, must be visible to any
 thread that later *acquires* this head pointer and follows it. Drop it to relaxed and a
 popper can legally see the new head pointer while the bytes behind it are still garbage.
 Failure order can stay relaxed, because a failed attempt reads nothing through the pointer.

### pop

```cpp
bool pop(T& out) {
    Node* old = head_.load(std::memory_order_acquire);
    while (old && !head_.compare_exchange_weak(old, old->next,
    std::memory_order_acquire,
    std::memory_order_acquire)) {
    }
    if (!old) return false;
    out = std::move(old->data);
    retire(old); // NOT delete, see below
    return true;
}
```

`acquire` on the load pairs with `release` in push: if this thread sees the pointer, it sees
the node's contents. The `old &&` short-circuit matters, on an empty stack `old` is null
and `old->next` must not be evaluated.

Note what the CAS gives you: **exclusive ownership of `old`.** Exactly one thread's
compare-exchange can succeed in unlinking a given node. So reading `old->data` afterwards is
not a race with any other popper. It is a race with *deallocation*, which is the next
section.

### Reclamation: the retire list

The version in this solution never frees a node while the stack is alive. Popped nodes go
onto a second lock-free list and are deleted in the destructor, when by contract only one
thread is left:

```cpp
struct Node {
    T data;
    Node* next = nullptr;
    Node* retire_next = nullptr; // a SEPARATE link, deliberately
};

void retire(Node* n) {
    n->retire_next = retired_.load(std::memory_order_relaxed);
    while (!retired_.compare_exchange_weak(n->retire_next, n,
    std::memory_order_release,
    std::memory_order_relaxed)) {
    }
}
```

The separate `retire_next` field is not tidiness. If retiring reused `next`, you would be
*writing* to `next` on a node that some other thread's in-flight compare-exchange is still
*reading*. That is a data race, and ThreadSanitizer will say so. Writing a field nobody else
touches is not.

This buys two properties for free:

- **No use-after-free**, because nothing is freed.
- **No ABA**, because an address is never handed back to the allocator, so the head can never
 become "the same pointer again". A stale `expected` can never compare equal.

And it costs one thing: memory grows without bound for as long as the stack lives. That is
not a real design; it is the smallest design that is *correct*, so that push, pop, and the
memory-ordering argument can be understood on their own before hazard pointers are layered
on top. Section 6 says exactly how it fails.

## 3. The broken version first

Three separate bugs live in the naive stack. It is worth seeing them one at a time, because
they fail in three completely different ways.

### 3a. push with a plain store: nodes vanish

```cpp
void push(T v) {
    Node* n = new Node(v);
    n->next = head_.load();
    head_.store(n); // <-- atomic, and still wrong
}
```

`head_` is `std::atomic`, so there is no data race and no torn pointer. It is still wrong,
because two threads can both load the same old head and then both store their own node: the
second store overwrites the first, and the first node, plus everything under it that was
not yet linked, is simply gone.

8 threads, 20,000 pushes each, counting the nodes actually reachable at the end:

```
push head.store(n): expected 160000 nodes, found 47150 (112850 lost, 70.5%)
push compare_exchange_weak: expected 160000 nodes, found 160000 (0 lost, 0.0%)
```

Seventy percent of the data thrown away, on a structure with no data race in it. "It's
atomic" protects the *word*; it does not protect the *transaction*.

### 3b. pop that ignores the compare-exchange result: duplicates

```cpp
Node* old = head_.load();
if (!old) return false;
head_.compare_exchange_strong(old, old->next); // return value discarded
out = old->data;
return true;
```

If the exchange fails, `head_` was not changed, but the code returns `old->data` anyway. Two
threads that both loaded the same head both return the same element, and everything below it
stays stranded on the stack forever. Four threads draining a 200,000-element stack:

```
pop ignoring the CAS result: 200003 pops of 200000 elements,
 97101 handed to more than one thread, 97098 never came out
```

Half the elements delivered twice, half never delivered. The fix is the retry loop.

### 3c. `delete old`: a use-after-free, not a lost update

This is the one worth slowing down for. Take a stack with a **correct** push and a
**correct** pop retry loop, and change exactly one thing, free the node you popped:

```cpp
bool pop(long& out) {
    Node* old = head.load(std::memory_order_acquire);
    while (old && !head.compare_exchange_weak(old, old->next,
    std::memory_order_acquire,
    std::memory_order_acquire)) {}
    if (!old) return false;
    out = old->v;
    delete old; // <-- the whole bug
    return true;
}
```

The window is between one thread's `head.load()` and its evaluation of `old->next`. In that
window another thread can pop that very node and free it. Now the first thread dereferences
freed memory, and on a busy allocator that memory has already been recycled into somebody
else's node and overwritten.

8 threads, 200,000 push/pop pairs each, compiled `-O2` with no sanitizer:

```
exit=139 exit=139 exit=139 exit=133 exit=139 exit=139
```

Six runs, six crashes. 139 is SIGSEGV, 133 is the allocator aborting on a corrupted heap.
The program does not produce a wrong answer; it stops existing.

ThreadSanitizer names it exactly, with the two ends of the window in different threads:

```
WARNING: ThreadSanitizer: data race
 Read of size 8 at 0x00010d300f78 by thread T2:
 #0 pop(long&) uaf.cpp:20 <-- reading old->next
 Previous write of size 8 at 0x00010d300f78 by thread T3:
 #0 push(long) uaf.cpp:12 <-- writing a NEW node into the same bytes
 Location is heap block of size 16 at 0x00010d300f70 allocated by thread T4
```

Read that carefully: a `pop` in one thread and a `push` in another are touching *the same
heap bytes*, because the block was freed and handed straight back out. The node identity you
thought you had was borrowed.

**Fix:** do not free while others may be looking. This solution's answer is the retire list.
Real answers are in section 4.

### 3d. ABA, the failure that survives even a correct-looking CAS

Suppose you dodge 3c somehow, or you get lucky, or you write your own pool allocator so
freed nodes are recycled instead of returned to the OS. There is still this:

> Thread A reads `head == X` and saves `old->next == Y`. Then A is descheduled.
> Other threads pop X, pop Y, and later push something that lands at address X again.
> `head` is X once more. A wakes up, its compare-exchange compares equal, and **succeeds**,
> setting `head` to Y, a node that is no longer part of the stack.

The compare-exchange did its job perfectly. The pointer *was* X. The problem is that "the
head is still X" was never the question A meant to ask; A meant "nothing has happened since
I looked", and a pointer value cannot express that.

Here is that schedule, forced deterministically with a node pool whose free list is LIFO, so
"the allocator hands the address back" is guaranteed rather than lucky:

```
recycling allocator (addresses get reused):
 start: stack is X(1) -> Y(2) -> Z(3), head=X
 A: read head=X, saved old->next=Y, then descheduled
 B: popped X (freed) and Y (kept out), head=Z
 B: pushed a new value, head=X again
 A: CAS(head, expect=X -> Y) SUCCEEDED
 final stack: Y(2) Z(3) (2 nodes)
 the value 99 that B pushed: GONE
 node Y, already handed to B as a popped element: BACK IN THE STACK, will be popped a second time

non-recycling allocator (an address is never handed back):
 start: stack is X(1) -> Y(2) -> Z(3), head=X
 A: read head=X, saved old->next=Y, then descheduled
 B: popped X (freed) and Y (kept out), head=Z
 B: pushed a new value, head=N3 again
 A: CAS(head, expect=N3 -> Y) failed, would retry
 final stack: N3(99) Z(3) (2 nodes)
 the value 99 that B pushed: present
 node Y, already handed to B as a popped element: correctly out
```

Same code, same schedule, same handshakes. The *only* difference is whether the allocator
ever hands an address back. In the first run a freshly pushed value disappears and a node
that was already delivered to a consumer is back at the head of the stack, waiting to be
delivered a second time.

Two things people get wrong about ABA:

- **It does not need a coincidence.** Allocators are LIFO by design, freeing a block and
 immediately allocating one of the same size very often gives you the same address back,
 because that is the cache-friendly thing to do. ABA is not a lottery; recycling is the
 common case.
- **It is not a data race.** TSan will not find it. Every access was atomic, every ordering
 was correct, and the answer is still wrong. This is a logic bug that lives above the
 memory model, which is why it survives the tools that catch everything else.

## 4. Real-world usage

**Where it came from.** R. Kent Treiber described this stack in an IBM report in 1986,
which is why it carries his name. The motivation was not "locks are slow" in the throughput
sense people usually mean. It was that a lock has an owner, and an owner can die, be
preempted, be swapped out, take a page fault, get killed by the scheduler, while holding
the lock, and then *nobody* makes progress. A compare-and-swap loop has no owner. Kill a
thread mid-push and the structure is untouched: either its CAS landed or it did not. That
property, not speed, is why lock-free structures exist in operating-system kernels,
interrupt handlers, signal handlers, and real-time systems.

The ABA problem was named in the IBM System/370 literature around the same time, for exactly
this reason: the 370's `CS` instruction made this style of programming possible, and the
first thing everybody hit was that comparing pointers is not the same as comparing histories.

**Where you actually meet it in production:**

- **Free-list and slab allocators.** The canonical use. A pool of fixed-size blocks with a
 lock-free LIFO free list is a Treiber stack, and, nicely, the reclamation problem
 disappears, because the "nodes" are the blocks themselves and are never returned to the OS.
 ABA very much does not disappear, which is why these use tagged pointers.
- **Work-stealing schedulers.** The private end of a worker's deque is often this shape.
- **Object pools in game engines and audio callbacks**, where a thread must not block and
 must not allocate.
- **`std::atomic<T*>` free lists inside the standard library and inside allocators**,
 tcmalloc, jemalloc, and friends all have a version of this internally.

**Where NOT to use it:**

- **When a mutex will do.** See section 5, under contention on this machine, a mutex-guarded
 linked stack beats this lock-free one by 3–8x. That is not a typo and it is not unusual.
 Lock-free is for latency guarantees and preemption tolerance, not for throughput.
- **When the queue discipline matters.** This is a *stack*. Under load it is unfair by
 construction: hot elements ping-pong at the head and the elements underneath can starve.
 If you wanted FIFO, this is the wrong structure and the lock-free FIFO (Michael–Scott
 queue) is substantially harder.
- **When you need more than the head.** Anything requiring two pointers to change together
 , a doubly-linked list, a size counter that is consistent with the contents, cannot be
 done with one CAS. `size()` on a lock-free stack is a lie the moment it returns.
- **When you have not solved reclamation.** Which, if you are reaching for this because it
 looked short, you have not. See the next paragraph and section 6.

**The three real answers to reclamation**, none of which is in this solution:

1. **Reference counting.** Keep a count of threads currently inside `pop`, and only free
 nodes when it drops to zero. Simple to explain, and it puts a second contended atomic on
 the hot path, so it is often slower than the lock it replaced. `std::atomic<std::shared_ptr<T>>`
 is the standard-library version of this idea, but note that on this machine
 (Apple clang 21, its bundled libc++) the `std::atomic<std::shared_ptr<T>>` specialisation
 is **not implemented at all**: `std::atomic<T>` static-asserts that `T` is trivially
 copyable, and `shared_ptr` is not. The older free-function form compiles, and
 `std::atomic_is_lock_free(&sp)` returns **0**, it is a mutex in disguise.
2. **Hazard pointers.** Every thread publishes, in a slot only it writes, the pointer it is
 currently about to dereference. A retiring thread scans all published slots; if its node
 appears in none of them, nobody can be looking at it and it is safe to free, otherwise it
 goes on a local list to try again later. This is the standard production answer, it is
 wait-free on the read side, and it is the subject of the `std::hazard_pointer` proposal
 (P2530) heading into the standard library. Today you use folly's `hazptr`, the
 Concurrency TS implementation, or your own.
3. **Epoch-based reclamation (RCU).** Threads announce entry and exit from a critical
 region; memory retired in epoch N is freed once every thread has been seen outside epoch
 N. Cheapest read side of the three, often literally free, but a single stalled thread
 holds up reclamation for everyone, so memory usage is unbounded in the presence of a
 straggler. This is how the Linux kernel does it.

The retire list in this solution is the degenerate case of (3): an epoch that only ever ends
when the object is destroyed.

## 5. Performance

All figures are this machine, Apple clang 21, arm64, 10 cores, `-O2`. Each thread runs a
push immediately followed by a pop, which is the maximum-contention workload: every single
operation touches the one head pointer. Best of three runs.

```
push+pop pairs, ns per operation (lower is better)
threads lock-free mutex speedup
1 8.4 15.3 1.83x
2 53.9 28.0 0.52x
4 123.8 52.8 0.43x
8 229.8 58.6 0.25x
10 314.2 46.3 0.15x
```

**The lock-free stack is 6 to 8 times slower than a mutex once more than one thread is
using it.** Read that again, because it is the opposite of what "lock-free" suggests, and it
is reproducible run to run.

Why. The retry count explains it:

```
CAS attempts per completed operation
 1 threads: 1.000
 2 threads: 1.289
 4 threads: 2.113
 8 threads: 4.727
 10 threads: 6.311
```

At 10 threads, six attempts per successful operation. Each failed attempt is not a cheap
spin: to run a compare-exchange, the core must pull the cache line holding `head_` into
exclusive state, which means taking it away from whichever core had it. Ten cores fighting
over one 64-byte line means the line is in flight essentially all the time, and five of every
six transfers accomplish nothing. This is the same physics as false sharing, except the
sharing is real and unavoidable, the head pointer *is* the shared state.

The mutex does better precisely because it does not do that. A blocked thread stops touching
the line at all. The critical section is three instructions, so the lock serialises the work
at roughly 46 ns per operation and *stays flat* from 4 threads to 10, while the lock-free
version degrades linearly with thread count.

It is not the retire list. Removing the second CAS entirely (abandoning popped nodes
outright) barely moves the contended numbers:

```
cost of the retire list (ns per push+pop operation)
threads no retire list with retire list
1 6.2 9.5
2 46.3 44.1
4 125.1 120.3
8 295.6 299.3
```

At one thread the extra CAS costs about 3 ns. Beyond that it is lost in the noise of head
contention.

**So what is lock-free actually buying?**

- **Single-threaded, 8.4 ns vs 15.3 ns**, nearly 2x, because an uncontended CAS is cheaper
 than an uncontended lock/unlock pair.
- **No thread can block another.** The mutex's flat 46 ns is an *average*. Any individual
 thread can be descheduled while holding the lock, and every other thread then waits for
 the scheduler, tens of microseconds, unbounded in principle. The lock-free version has no
 such tail. If you are writing an audio callback or an interrupt handler, that tail is the
 entire reason you are here, and the throughput table is irrelevant.
- **Safety in contexts where locking is illegal**: signal handlers, interrupt context, code
 that may be killed asynchronously.

The rule this measurement supports: **choose lock-free for the worst case, not the average
case.** If your metric is throughput, measure before you believe.

## 6. Where this solution fails

**1. It never frees memory while the stack is alive. This is the big one.**
Every popped node stays on the retire list until the destructor runs. A server with a
long-lived stack handling a million operations a second will grow by tens of megabytes per
second, forever, and eventually be killed by the OOM killer. This solution is correct and
unusable in production for exactly this reason. The honest description is: *the memory
reclamation problem has been deferred, not solved.* A real implementation needs hazard
pointers (each thread publishes the node it is about to dereference; retiring threads scan
those publications and free only unpublished nodes) or epoch-based reclamation. Both roughly
double the size of the code and add a per-operation cost to the read side.

**2. `pop` is lock-free, not wait-free.** An individual thread can lose its
compare-exchange arbitrarily many times in a row. The *system* always makes progress,
somebody's CAS succeeds every round, but no bound exists on how long any one thread takes.
The measured 6.3 attempts per operation at 10 threads is the average; the tail is much
worse. If you need a bound per thread, you need a wait-free algorithm, which is a different
and much harder design.

**3. Throughput collapses under contention, as measured.** 8x slower than a mutex at 10
threads. If you adopted this to make a hot path faster, you made it slower. It is the right
tool for latency tails and for contexts where blocking is forbidden, and the wrong tool for
raw throughput.

**4. Exception safety in `pop`.** `out = std::move(old->data)` runs *after* the node has been
unlinked. If `T`'s move assignment throws, the element is already off the stack and is lost,
it is not on the stack and it is not in `out`. This is the same hazard as the classic
`pop()`-returning-by-value problem; the reference-out-parameter interface reduces it but does
not eliminate it for types whose move can throw. For `T` with a `noexcept` move (which
includes every trivially copyable type) the problem does not arise.

**5. The destructor's contract is unenforced.** `~LockFreeStack` walks both lists with no
synchronisation beyond its acquire loads. If any thread is still pushing or popping while the
destructor runs, the behaviour is undefined and the acquire loads will not save you. Nothing
in the type prevents this; it is a comment, not a guarantee.

**6. `empty()` is only ever a historical fact.** By the time it returns, the answer may be
wrong. `if (!s.empty()) s.pop(v);` is the classic interface race, it can still fail. Anything
built on `empty()` for control flow is broken by construction; use `pop`'s return value.

**7. No ABA protection *of its own*.** ABA is avoided here purely as a side effect of never
recycling addresses. Add any form of node reuse, a free list, a pool allocator, an arena,
and the ABA scenario in section 3d is live again, silently, with no sanitizer to catch it.
The standard defences are a **tagged pointer** (pack a monotonically increasing counter next
to the pointer so "same pointer, later time" compares unequal) or a reclamation scheme that
prevents reuse while readers exist. Worth knowing for this machine specifically: a 16-byte
`struct { void* p; unsigned long tag; }` *is* genuinely lock-free here,
`std::atomic<Tagged>::is_always_lock_free` is true and the compiler emits a single `caspal`
(128-bit compare-and-swap) instruction, so tagged pointers are a real option on arm64 rather
than a theoretical one. Note that a tag solves ABA and does **not** solve use-after-free:
you still may not dereference a freed node, tagged or not.

**8. Unbounded allocation on `push`.** Every push calls `new`, which on most implementations
takes a lock somewhere inside the allocator. A "lock-free" structure whose hot path calls
`operator new` is not lock-free in the sense that matters to a real-time thread. Production
versions preallocate nodes from a pool, which, as above, brings ABA straight back.

**9. `T` must be movable, and the node holds a `T` by value.** A large `T` makes each node
large and each push a bigger allocation. Nothing here is wrong; it is just that the memory
profile is worse than a `std::vector`-backed stack under a mutex, which is what the
alternative would actually be.

## 7. Interview follow-ups

**"You measured lock-free losing to a mutex by 6-8x under contention. Given that, why would
anyone actually reach for a Treiber stack in production?"** Not for throughput, the
measurement is explicit that a mutex wins there, flat at ~46ns regardless of thread count
while lock-free degrades linearly. The real reason is the *tail*, not the average: a mutex
has an owner, and that owner can be preempted, page-fault, or get killed while holding the
lock, and then every other thread stalls waiting for the scheduler, an unbounded tail in
principle. A CAS loop has no owner; kill a thread mid-push and the structure is untouched.
That property, not speed, is why lock-free structures exist in interrupt handlers, signal
handlers, and real-time audio callbacks, where blocking (even briefly) is either illegal or
catastrophic, and average-case throughput is the wrong metric entirely.

**"ABA doesn't need a coincidence, you said recycling is the common case, not a lottery. Why
would a production allocator make this MORE likely, not less?"** Allocators are LIFO by
design for cache-friendliness, freeing a block and immediately allocating one of the same
size very often returns the exact same address, because that's the address most likely to
still be warm in cache. That's a deliberate performance optimization in the allocator,
and it's precisely what turns "address gets reused" from a rare edge case into the expected,
common-case behavior, meaning a naive Treiber stack over a recycling allocator will hit ABA
reliably under real load, not occasionally under bad luck.

**"You measured ThreadSanitizer catching the use-after-free bug but noted ABA is invisible to
it. Walk through why the same tool catches one and misses the other."** Use-after-free (3c)
is a genuine data race by TSan's definition: two threads touch the same memory with no
happens-before relationship between them (a pop's read racing a push's write into freed-then-
recycled bytes), which is exactly what TSan is built to detect. ABA (3d) has no such race,
every access is a properly ordered atomic load or CAS, the pointer values are read correctly,
and the CAS genuinely succeeds because the value it compared against genuinely matched. The
bug is that "the pointer equals X" was never actually equivalent to "nothing has happened
since I last looked," and no per-access race detector can check a claim about program
*history*, only about individual memory accesses. This is the same category of tool-blind
bug [[013-store-buffering]] and [[043-testing-concurrent-code]]'s readings describe for
memory-ordering bugs, arrived at from a completely different direction.

**"Small machine, this machine happened to have single-instruction CAS (LSE atomics), so
weak vs strong measured identically. What would change on a machine without that?"** On a
classic load-linked/store-conditional architecture (older ARM, POWER, RISC-V without LSE),
`compare_exchange_strong` has to internally loop to hide spurious store-conditional failures
that have nothing to do with your value (an interrupt, another core touching an unrelated
variable on the same cache line), `compare_exchange_weak` reports that failure directly and
lets your own retry loop absorb it for free. Inside a loop you were already going to retry
either way, so `_strong`'s extra hidden loop buys nothing there and costs a real instruction
or two per attempt, which is exactly why the rule ("`_weak` inside a loop") holds even on
this machine where the two happened to compile identically; it's free insurance for every
other target the same code will run on.

**"Production ops, this stack never frees memory while it's alive, by design, and you called
that 'correct and unusable in production.' How would you actually detect this is happening
before an OOM kill, and what would you page on?"** Track resident memory growth rate against
the stack's own push/pop throughput as a derived metric, a healthy bounded structure shows
flat memory over time regardless of operation rate; this design's retire list grows
monotonically with total pops, so memory-per-pop trending upward without bound (rather than
plateauing) is the specific signature to alert on, well before the OOM killer's blunt
last-resort signal. The real fix, per section 4, is hazard pointers or epoch-based
reclamation, this retire-list version exists specifically so push/pop/memory-ordering can be
understood in isolation before that additional machinery is layered on.
