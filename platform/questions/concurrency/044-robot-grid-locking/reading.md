## 1. Reframe the problem

A grid where many robots can each be doing something in a different cell is an obvious
candidate for one mutex per cell, fine-grained locking, maximum concurrency, robots working
on opposite corners of the grid never wait on each other at all. The trap isn't the
granularity, it's that a *move* touches two cells at once, and "lock the one I'm leaving,
then lock the one I'm entering" reintroduces the exact two-mutex deadlock this course has
already seen in [[005-scoped-lock]], just dressed up as robots instead of bank accounts.
The fix is the same fix, because the shape of the problem is the same: two threads, two
locks, no control over which order the *other* thread happens to acquire them in.

## 3. The broken version, first

The boilerplate locks the current cell, then the target cell, while still holding the
first:

```cpp
std::lock_guard<std::mutex> lk1(from);
std::lock_guard<std::mutex> lk2(to);
```

**Why it looks right:** it's the natural order to write, "leave here, arrive there" reads
left to right, and for a single robot moving alone, this is completely correct; nothing
breaks until a second robot is moving into the first robot's current cell at the same
moment.

Running it, 40 robot pairs, each pair repeatedly swapping into each other's cell at the
same time, 300 rounds:

```
TIMED OUT -- the program stopped making progress. Deadlock, or waiting on
something that never arrives.
```

Robot A (moving cell 1 -> cell 2) holds cell 1's lock, blocked waiting for cell 2. Robot B
(moving cell 2 -> cell 1), at the same moment, holds cell 2's lock, blocked waiting for cell
1. Neither can ever proceed, the textbook AB/BA deadlock, reliably reproduced because with
40 pairs racing simultaneously across 300 rounds each, the unlucky interleaving is essentially
guaranteed to occur somewhere.

The fix is `std::lock`, which acquires both mutexes together using an internal
deadlock-avoidance algorithm, it doesn't matter that robot A and robot B name their two
mutexes in opposite order, `std::lock` guarantees neither call can ever be the "AB" to the
other's "BA":

```cpp
std::lock(a, b);
std::lock_guard<std::mutex> lk1(a, std::adopt_lock);
std::lock_guard<std::mutex> lk2(b, std::adopt_lock);
```

Same 40-pair, 300-round swap storm:

```
40 robot pairs x 300 rounds of simultaneous cell-swapping: no deadlock
```

## 6. Where this solution fails

- **One mutex per cell is a real memory cost, and it's worse than it sounds.** Measured on
 this machine: `sizeof(std::mutex)` is **64 bytes** (the guide this course draws from
 claims ~40 bytes, implementation-dependent, and worth re-measuring on whatever platform
 you actually ship on rather than trusting a remembered number). A 1000x1000 grid is
 1,000,000 cells, **64 MB** of mutexes alone, before any actual grid data, and every one
 of those mutex objects is a small, scattered allocation that pollutes cache lines the
 actual cleaning logic never needs to touch.
- **Fine-grained locking at this scale trades memory and cache pressure for concurrency you
 may not need.** If robots are sparse relative to grid size, most of those million mutexes
 are never contended, the real fix at scale is usually **coarsening**: lock larger blocks
 (say 10x10 chunks) instead of individual cells, trading a little unnecessary serialization
 (two robots in the same block that aren't actually touching the same cell) for a 100x
 reduction in mutex count and a design that fits the way real hardware caches work.
- **A robot whose thread dies (or simply never calls the matching unlock, impossible here
 thanks to RAII, but very possible in a hand-rolled `lock()`/`unlock()` design) leaves its
 cell locked forever.** `std::lock_guard`/RAII already protects against a *thrown exception*
 leaving a lock held; it does nothing about a thread that's killed externally or hangs
 indefinitely mid-move. A production version guarding against that needs a `std::timed_mutex`
 and a deadline, the same idea as [[041-deadlock-detection-watchdogs]].
- **Two robots can still both want to move into the same, single target cell (not swap,
 genuine contention for one destination) at once**, and `std::lock` doesn't decide who wins
 that race, only that neither deadlocks while trying. Whichever robot's `std::lock` call
 happens to succeed first gets the cell; the design says nothing about fairness between
 competing robots, the same open question [[021-dining-philosophers]]'s reading raises for
 a very similar resource-sharing shape.

## 7. Interview follow-ups

**"Why not just have every robot acquire cell locks in a fixed global order (say, by row-
major index) instead of using std::lock?"** That works too, and is arguably simpler to
reason about, it's the same "impose a total order on lock acquisition" technique
[[021-dining-philosophers]] uses. `std::lock`'s advantage is that it doesn't require you to
sort or compare the two mutexes yourself; it works correctly regardless of what order the
caller happens to name them in, which matters when the "natural" order to write the code in
(current cell first, target cell second) is different from whatever consistent order you'd
otherwise have to impose by hand.

**"At what grid size does per-cell locking stop being a reasonable design, and what would
you actually build instead?"** Once the mutex array's memory footprint or cache pollution
starts to matter more than the concurrency it buys, which the measurement above puts well
within reach of a 1000x1000 grid, chunk-based locking (lock 10x10 or 100x100 blocks) is the
direct fix, trading some unnecessary contention (two robots in the same block but different
cells now wait on each other) for a much smaller, cache-friendlier lock array. A lock-free
design (robots claim cells via atomic CAS on a per-cell "owner" field, no `std::mutex` at
all) is the next step past that, at real implementation complexity cost.

**"A robot's battery dies mid-move, while it holds a cell's lock, what actually happens in
this design, and how would you detect it?"** With plain `std::mutex` and RAII, "dies" mid-
function only unlocks correctly if the thread still runs its destructors (a normal return,
or an exception unwinding), a genuinely killed thread, or one that hangs forever without
ever leaving the locked scope, leaves that cell locked for the life of the program, exactly
like [[041-deadlock-detection-watchdogs]]'s core scenario. The guide's own suggested fix
(a `std::timed_mutex` with `try_lock_for`, treating a timeout as "this robot is dead, abandon
the path") is the direct application of that question's watchdog idea to this one.
