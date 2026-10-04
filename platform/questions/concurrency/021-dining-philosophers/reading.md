## 1. Reframe the problem

Five philosophers, five forks, each fork shared by two neighbours. Nobody eats without both
their forks, and nobody hands you a way to check "is this fork free?" without first asking
for it. The instinct is symmetry: every philosopher reaches for their left fork, then their
right. It's the natural way to write the code, it treats everyone identically, and there is
no data race anywhere in it, each fork has its own mutex, correctly.

It still deadlocks, every time, under load. Symmetry is exactly the bug. If all five
philosophers reach left at the same instant, all five now hold one fork and are waiting for
a fork their neighbour is holding, a ring of five threads, each one step from finishing,
each blocked on the thread next to it. This is the textbook shape of **circular wait**, one
of four conditions ([[006-hierarchical-mutex]] covers the general theory) that must *all*
hold for a deadlock to exist: mutual exclusion (a fork can't be shared), hold-and-wait
(keep your first fork while asking for the second), no preemption (nobody can be forced to
give a fork up), and circular wait (a closed loop of "waiting for the next guy's resource").
You cannot remove mutual exclusion, a fork genuinely can't be split. You cannot remove
hold-and-wait without redesigning the interface to ask for both forks atomically. The one
condition that's cheap to break is circular wait, and breaking *any one* of the four is
enough to make deadlock impossible.

Breaking circular wait means: no cycle can exist if every thread that ever wants two
resources always asks for them in the same global order. If everyone acquires forks from
low index to high index, a cycle would need someone waiting on a fork with a *smaller* index
than one they already hold, and nobody ever does that, so the cycle can't close. The whole
fix is one philosopher, at one seat, taking their forks in the opposite order from everyone
else.

## 2. The tools

### One mutex per fork

```cpp
std::mutex forks_[5];
```

This part of the naive version is already right: a fork is a resource with an owner, and a
`std::mutex` is exactly "at most one owner at a time." The bug isn't the data structure,
it's the *order* every philosopher acquires it in.

### Lock ordering as a deadlock-avoidance strategy

The general rule: give every lockable resource in the program a total order (here, its
index 0–4), and never acquire a lock that is lower in the order than one you already hold.
Two threads that both follow the rule can never deadlock against each other, because a
deadlock needs a cycle, and a cycle needs at least one edge that goes "backwards" through
the order, which the rule forbids.

Applying it here: philosopher `i`'s left fork is fork `i`, right fork is fork `(i+4) % 5`
(their neighbour's left fork, one seat over). For philosophers 1 through 4, `left > right`
already, fork `i` then fork `i-1` is already descending, no change needed. Philosopher 0 is
the seam: left is fork 0, right is fork 4, and taking them in the "natural" left-then-right
order means taking the *smallest* index first, an ascending acquisition, breaking the rule
every one of the other four is following. Flip philosopher 0 alone to take fork 4 before
fork 0, and now every philosopher, without exception, acquires forks in descending index
order. No thread ever waits on a fork with a lower index than one it holds, so no cycle can
close.

```cpp
if (philosopher == 0) {
    forks_[right].lock(); pickRightFork();
    forks_[left].lock(); pickLeftFork();
} else {
    forks_[left].lock(); pickLeftFork();
    forks_[right].lock(); pickRightFork();
}
```

One seat is special-cased; the other four are untouched. That's the entire fix, nothing
about the mutexes themselves changed.

### Why this doesn't need std::scoped_lock

[[005-scoped-lock]] solves a *different* problem: two threads that might each try to lock
the same two mutexes in opposite orders, where you can't fix the order in the source because
which mutex is "first" depends on runtime arguments (e.g. transferring between two
accounts). Here every philosopher's fork indices are known at compile time relative to their
seat, so a static per-seat ordering is enough, no need for `scoped_lock`'s runtime
deadlock-avoidance algorithm.

## 3. The broken version, first

The boilerplate is the version everyone writes on the first pass, five identical lines,
each fork correctly protected by its own mutex, symmetric across all five philosophers:

```cpp
forks_[left].lock();
pickLeftFork();
forks_[right].lock();
pickRightFork();
```

**Why it looks right:** it treats every philosopher identically, which reads as *more*
correct than special-casing one of them, no seat is privileged, the code has no branches,
and there is genuinely no data race: every fork access is inside that fork's own lock. It
compiles clean, passes a quick single-threaded smoke test (one philosopher eating alone
never contends with anyone), and the bug only appears when enough threads reach the table at
the same moment.

Running the test, 5 threads launched together, 40 meals each, under a 3-second wall-clock
limit:

```
(no output, killed after 3s, exit 142/SIGALRM)
```

Every philosopher grabbed their left fork and is now waiting for their right fork, held by
their right-hand neighbour, who is doing the exact same thing. Five threads, five forks, all
held, nobody can proceed, the classic deadlock, and TSan has nothing to say about it,
because there is no race to report. `./check`'s stress stage catches this the way it catches
every deadlock in this course: not by detecting a race, but by timing out and reporting the
hang.

The fix, flip philosopher 0's acquisition order, same test, same 5 threads, 40 meals each,
6 trials:

```
6 trials x 5 philosophers x 40 meals: everyone ate, no neighbours ever shared a fork, up to 2 ate at once
```

Nothing about the fork objects changed. The only difference between a program that always
deadlocks and one that never does is which order one out of five threads asks for its locks.

## 4. Real-world usage

**Why it was invented.** Edsger Dijkstra posed the dining philosophers problem in 1965 as a
teaching example, originally about five computers competing for access to five shared tape
drives, specifically to make circular wait *visible*. Every other deadlock condition is
easy to see by inspection; a cycle of "I hold X, waiting for Y" relationships is easy to
introduce by accident and easy to miss in review, because each individual thread's code
looks completely correct in isolation. The problem's value isn't the puzzle, it's that it
forces you to reason about the *whole system's* lock-acquisition graph, not any one thread's
logic.

**Where you meet the same shape in production:**

- **Any multi-resource transaction.** A database transaction that locks row A then row B,
 racing against a transaction that locks B then A, is dining philosophers with two
 philosophers and two forks. This is why real databases implement deadlock *detection*
 (a wait-for graph, checked periodically) as well as prevention, lock ordering isn't
 always practical when the resources involved are chosen by user queries at runtime.
- **Distributed resource allocation.** Two services each needing a lease on two shards, or
 two shared caches, acquired in different orders under load, same failure, harder to
 debug because "grab the debugger and look at five stacks" doesn't work across machines.
- **Bank transfers.** [[005-scoped-lock]]'s exact problem, two accounts, two mutexes,
 order determined by which account each caller mentions first.

**Where NOT to reach for global lock ordering:**

- **When the resource set isn't known upfront.** Lock ordering needs *some* stable total
 order to sort by. If resources are created dynamically and compared by, say, pointer
 address, the "order" changes across runs and becomes hard to reason about or test
 deterministically, prefer a comparison key that's stable and meaningful (an id, not an
 address) or use `std::scoped_lock`'s runtime algorithm instead of a hand-rolled order.
- **When contention is the actual problem, not deadlock.** Lock ordering fixes deadlock, not
 throughput. If five threads are constantly fighting over the same handful of locks, the
 fix is reducing contention (finer-grained locks, sharding, lock-free structures), and
 ordering them correctly just means they queue up safely instead of deadlocking.

## 5. Performance

Measured on this machine, 5 philosopher threads, 6 trials x 40 meals each (the test's own
workload): the resource-hierarchy fix never deadlocks, and non-neighbouring philosophers
genuinely overlap, **peak concurrency observed: 2 philosophers eating simultaneously**
(the maximum possible with 5 seats around a table, since any 3 simultaneous eaters would
require two of them to be neighbours).

**Fairness, measured separately** (20,000 meals per philosopher, no artificial neighbour
contention beyond the table itself, average wait-to-acquire-both-forks per meal):

```
philosopher 0: avg wait-to-eat 441 ns
philosopher 1: avg wait-to-eat 269 ns
philosopher 2: avg wait-to-eat 369 ns
philosopher 3: avg wait-to-eat 505 ns
philosopher 4: avg wait-to-eat 594 ns
```

Under this uniform, no-adversarial-scheduling workload there's no dramatic starvation of
seat 0, the guide's theoretical concern (see section 7) doesn't show up strongly here, and
the ordering (4 slowest, not 0) doesn't even match the "philosopher 0 is disadvantaged"
story cleanly. That's a useful result on its own: **fairness isn't something this design
guarantees, it's something this particular run happened not to violate.** Section 6 covers
why the guarantee is genuinely absent.

## 6. Where this solution fails

- **No fairness guarantee, only a deadlock guarantee.** Breaking circular wait proves no
 cycle can *ever* form, it says nothing about how long any individual philosopher waits.
 Under adversarial timing (four fast philosophers that always happen to grab their forks a
 few nanoseconds before the fifth), the resource-hierarchy solution has no mechanism to
 stop one seat from being consistently last. The measurement above shows this doesn't
 happen under uniform contention; it doesn't show it can't happen.
- **A throwing `eat()` callback leaks a held fork forever.** The forks are locked and
 unlocked by hand (`.lock()` / `.unlock()`), not with `std::lock_guard`. If `eat()` throws,
 the stack unwinds past both `unlock()` calls and the philosopher's forks stay locked,
 every neighbour who ever needs either fork blocks permanently. This is a real gap in the
 solution as written; production code should wrap the locked section in RAII (`lock_guard`
 per fork, or `scoped_lock` for both at once) specifically so an exception mid-meal releases
 what was held instead of leaking it.
- **The special case is easy to break by refactoring.** The entire fix lives in one
 `if (philosopher == 0)` branch. Someone generalizing this to N philosophers, or reordering
 seats, has to remember *why* exactly one seat is different, nothing in the type system
 enforces "exactly one participant in a cyclic resource graph must break the cycle." A
 comment is the only thing standing between a future edit and reintroducing the deadlock.
- **Doesn't generalize past "each thread wants exactly two specific, adjacent resources."**
 The moment a philosopher might need three forks, or the demand graph isn't a simple ring,
 "sort by index" needs re-deriving from the actual resource graph, not just copied. General
 lock-ordering code typically sorts the *actual set of locks requested this call* rather
 than hard-coding per-seat branches, closer to what `std::scoped_lock(a, b, c, ...)` does
 internally.
- **A concierge/semaphore alternative trades fairness for throughput, not for free.** The
 guide's suggested fix, a counting semaphore initialized to 4, so at most 4 of the 5
 philosophers can even attempt to pick up forks at once, guarantees at least one seat can
 always make progress (there's always at least one fork-pair uncontested among 4 out of 5
 seats). It's a real fix for the theoretical starvation case, but it adds a second
 synchronisation primitive and a second point of contention (the semaphore itself) purely
 to buy a guarantee the lock-ordering fix doesn't need for correctness, worth it only if
 fairness is an actual product requirement, not by default.

## 7. Interview follow-ups

**"What are the four necessary conditions for deadlock, and which one does this fix break?"**
Mutual exclusion, hold-and-wait, no preemption, circular wait, all four must hold
simultaneously for deadlock to be possible. This fix breaks circular wait specifically, by
imposing a global acquisition order on the forks so no thread ever waits on a resource
"behind" one it already holds.

**"Does this maximize throughput, is peak-2-eating-at-once the best you can do?"** With 5
seats around a ring, 2 is actually the ceiling for simultaneous eaters (any 3 must include
two neighbours sharing a fork), so the measurement above is already at the structural
maximum. The guide's throughput concern is really about *fairness*, not peak
concurrency, whether philosopher 0 in particular gets a fair share of the 2-at-a-time slots
over time, which section 6 covers.

**"Small machine, 1 physical core, 5 threads time-sliced.** Does the deadlock risk change?"
No, deadlock is a property of the lock-acquisition order, not of scheduling. A single-core
machine still preempts threads mid-function via context switches, so the same interleaving
that causes all five to hold their left fork simultaneously is reachable even with no real
parallelism. What changes on one core is throughput (no genuine overlap is *possible*, so
"peak eating at once" would measure 1, not 2), but the naive version's deadlock and the
fix's absence of one are both scheduling-independent.

**"Big machine, 500 philosophers, 500 forks, heavy contention. What breaks first?"** The
lock-ordering fix still prevents deadlock at any N, the proof doesn't depend on N=5. What
degrades is fairness variance (more seats competing means more opportunity for one seat to
be unlucky repeatedly) and cache behaviour: 500 `std::mutex` objects contended from many
cores means more cache-line bouncing on whichever forks are in the "hot" part of the ring at
a given moment. The fix that matters here is the same one section 6 names for contention in
general, reduce how often threads actually collide, not a different deadlock strategy.

**"A philosopher's `eat()` callback throws partway through, walk through what happens and
how you'd fix it."** As covered in section 6: the stack unwinds past the manual `.unlock()`
calls, both forks this philosopher holds stay locked, and every neighbour that later needs
either fork blocks forever, the *program* doesn't crash, it develops a slow, silent
deadlock centered on one seat. The fix is RAII: wrap the two `lock()`/`unlock()` pairs in
`std::lock_guard` (or take both forks via `std::scoped_lock` in the already-correct order)
so a thrown exception still runs the destructors and releases both forks during unwind.

**"Production ops: how would you tell 'the table is deadlocked' from 'the table is just
slow' from monitoring alone, without attaching a debugger?"** Track per-philosopher
wait-to-eat latency (the section 5 measurement) as a live metric with an alerting threshold,
plus a simple liveness check, a counter of total meals served, alerted if it stops
increasing for longer than the slowest plausible legitimate wait. A true deadlock shows as
that counter going flat forever with several threads simultaneously blocked on `lock()`;
mere slowness shows as latency climbing but the counter still moving. This is the same
principle `./check`'s own stress stage uses: a hang isn't detected by inspecting locks, it's
detected by a watchdog timing out an expected-to-finish operation.
