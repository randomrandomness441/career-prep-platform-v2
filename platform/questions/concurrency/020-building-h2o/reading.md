## 1. Reframe the problem

The chemistry is decoration. Strip it away and the problem is this: **a shared output stream
must be cut into fixed-shape batches, and the threads producing it are of two kinds with a
quota each.**

That shape shows up constantly once you recognise it, a writer that must emit a record
header followed by exactly N fields, a network layer that must not start frame *k+1* until
frame *k* has been fully written, a log formatter where two producers interleave into one
line. Every one of them is "two hydrogens and an oxygen, then start again".

Three observations turn it into something small enough to hold in your head.

**Nobody needs to be paired with anybody.** A hydrogen does not have to find "its" oxygen.
There is no matchmaking, no identity, no handshake between specific threads. The only state
in the whole problem is *how much of the current group has already gone out*. Two integers.
Threads are interchangeable.

**The condition to wait on is a quota, not a turn.** A hydrogen asks "does the molecule
under construction still have a hydrogen slot free?", that is `h_ < 2`. An oxygen asks
`o_ < 1`. Neither has to care what the other count is. That is why this problem, which
sounds like it needs a state machine over six orderings (`HHO`, `HOH`, `OHH`, and so on),
needs no state machine at all. Any interleaving that respects both quotas is a legal
molecule, and there are exactly two quotas to respect.

**The group boundary is the moment both quotas are full.** `h_ == 2 && o_ == 1` means the
molecule is done, so both counters go back to zero and the threads queued behind it are
allowed to look again. The reset *is* the boundary, there is no separate "phase" variable.

So the design is: a mutex, a condition variable, two counters, and a reset. The reason this
question is not trivial despite that is the fourth observation, which is about placement
rather than logic:

**The counters describe the output only if nothing can emit an atom in the gap between the
check and the count.** `releaseHydrogen()` is the observable event. `++h_` is the
bookkeeping. If any other thread can slip between them, your bookkeeping is describing a
past that no longer matches the tape. So the release goes *inside* the critical section,
between the wait and the increment. That is unusual advice, the standard instinct is to do
as little as possible while holding a lock, and especially never to invoke a caller-supplied
callback there, and section 6 is about the price you pay for it.

## 3. The broken version, first

Here is the version that gets written first, and it is not stupid. It locks. It notifies.
It has no data race whatsoever.

```cpp
void hydrogen(std::function<void()> releaseHydrogen) {
    std::unique_lock<std::mutex> lk(m_);
    releaseHydrogen();
    cv_.notify_all();
}
```

Forty hydrogen threads and twenty oxygen threads, hydrogens started first. Three
consecutive runs of the real program:

```
naive run 1: first 24 atoms HHHHHHHHHHHHHHHHHHHHHHHH
 20 of 20 groups are not H2O
naive run 2: first 24 atoms HHHHHHHHHHHHHHHHHHHHHHHH
 20 of 20 groups are not H2O
naive run 3: first 24 atoms HHHHHHHHHHHHHHHHHHHHHHHH
 20 of 20 groups are not H2O
```

**Twenty out of twenty groups wrong, every run.** Not "sometimes racy", the naive version
produces forty hydrogens followed by twenty oxygens, which is the worst possible answer, and
it does it deterministically.

Why it fails is worth stating precisely, because the mutex being present is what makes it
confusing. The mutex enforces *mutual exclusion*: at most one atom is emitted at a time, and
the tape is never corrupted. What it does not enforce is *ordering between kinds*. Mutual
exclusion answers "can these two lines run at the same time"; this problem asks "is this
thread allowed to run **yet**". Those are different questions and a mutex only answers the
first. Waiting for a condition to become true is what a condition variable is for, and the
naive code has one but never waits on it, it only ever notifies, which is signalling into
an empty room.

Now the more interesting broken version, the one you write on the second attempt, after you
have added the counters:

```cpp
void hydrogen(std::function<void()> releaseHydrogen) {
    {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return h_ < 2; });
        ++h_;
        if (h_ == 2 && o_ == 1) { h_ = 0; o_ = 0; }
    }
    cv_.notify_all();
    releaseHydrogen(); // moved out of the lock, "to be tidy"
}
```

Every instinct you have about lock scope says this is an improvement: do the accounting
under the lock, do the slow user callback outside it. The counters are still perfectly
correct, at no point do more than two hydrogens hold a slot in the same molecule.

The output is still wrong. Same 40 hydrogens and 20 oxygens, five runs:

```
tidy run 1: first 24 atoms HHOHOHHOHHHHOHOHHHOOHHOH 8 of 20 groups are not H2O
tidy run 2: first 24 atoms HHOOHHHHHHOOHOHHHOHHOOHO 6 of 20 groups are not H2O
tidy run 3: first 24 atoms HHOHHOHHOHHOOHOHHHOHHOHH 8 of 20 groups are not H2O
tidy run 4: first 24 atoms HHHOHOHHOHHOHHOHHOHHHHOO 8 of 20 groups are not H2O
tidy run 5: first 24 atoms HHOOHHHHHOHHHOOHOHHHHHOO 8 of 20 groups are not H2O
```

Notice how much *better* it looks than the first version, long stretches are perfectly
valid molecules, and the failure rate has dropped from 100% of groups to around a third.
That is the dangerous kind of broken. It would pass a lazy eyeball test, and on a
single-core machine or under light load it might pass a whole test suite.

The reason is that the counters were never the thing being tested. Two
hydrogens claim slots in molecule 5 and both are descheduled before their `releaseHydrogen`
call. An oxygen claims its slot, completes molecule 5, resets, and two hydrogens of molecule
6 claim slots and emit first. The tape shows molecule 6's hydrogens before molecule 5's, and
the group of three straddling the boundary contains three hydrogens. The reservation was
correct; the *emission order* was not, and the emission order is the whole specification.

That is the lesson worth carrying out of this question: **a lock protects the state you put
inside it, and nothing else.** If the observable event lives outside the critical section,
you have serialised your bookkeeping and left the thing your users see unordered.

## 6. Where this solution fails

- **It calls a caller-supplied callback while holding the mutex, and it has to.** This is
 normally a firing offence, the callback can block, can take other locks, can throw, and
 can call back into `H2O` and self-deadlock on a non-recursive mutex. Here it is
 unavoidable: the callback *is* the observable event whose order you are specifying, so it
 cannot leave the critical section. The honest way to state the constraint is that `H2O`
 requires its callbacks to be short, non-blocking, and to touch nothing that could lead back
 to `H2O`. If you cannot promise that, this interface is the wrong interface, you want the
 callback to hand you a value and let `H2O` own the writing.

- **Throughput is capped at one atom at a time.** Everything happens under one mutex, so the
 assembler has no parallelism at all; it is a serialisation point that gets slower as you
 add threads, because more of them pile onto the same lock. That is inherent to the problem
 as specified, a total order on the output *is* a serialisation, but it means "make it
 faster with more threads" is not available. If the real goal were throughput you would
 batch: have each producer hand over its atom and let one writer thread emit whole molecules.

- **`notify_all` wakes everyone to let almost nobody through.** Completing a molecule frees
 two hydrogen slots, so 38 blocked hydrogen threads wake, 2 proceed and 36 re-check their
 predicate and go back to sleep. That is the thundering herd, and it is O(waiters) wasted
 wakeups per molecule. `notify_one` is not a valid substitute, it can wake the single
 oxygen when what was needed was two hydrogens, and then nothing wakes anybody again. If
 the herd is measurably hurting you the fix is two condition variables, one per kind, so a
 completed molecule can notify hydrogens twice and oxygens once and disturb nobody else.

- **It assumes a supply that can actually form molecules.** Feed it three hydrogens and no
 oxygen and two of them block forever. Nothing in the code detects "this can never
 complete", and nothing can, the assembler cannot know whether an oxygen is one microsecond
 or one hour away. A production version needs a timeout or a `stop_token` and a defined
 answer for the leftover atoms, and "what do we do with a partial molecule at shutdown" is a
 policy question the synchronisation cannot answer for you.

- **No fairness, and no bound on how long any one thread waits.** `std::condition_variable`
 makes no ordering promise about which waiter wins. A hydrogen that arrived first can lose
 every race to hydrogens that arrived later, indefinitely. For a fixed batch of work that is
 invisible, because everyone finishes. For a long-running stream it means an unlucky thread
 can starve, and no amount of `notify_all` fixes it, you would need an explicit ticket
 queue.

- **The obvious "modern C++20" rewrite has a real deadlock in it.** The tempting reformulation
 is to make the quotas semaphores and the group boundary a barrier, restocking the permits
 from the barrier's completion function:

 ```cpp
std::counting_semaphore<2> h_slots_{2};
std::counting_semaphore<1> o_slots_{1};
std::barrier<Restock> group_{3, Restock{this}}; // Restock releases 2 H and 1 O
// hydrogen: h_slots_.acquire(); release(); group_.arrive_and_wait();

```

 It reads beautifully and it hangs. Measured on this machine, three runs out of three, it
 stopped dead after exactly two molecules, six atoms out of sixty. Restocking inside the
 completion function hands out next-phase permits *during* the barrier's completion step, so
 a thread can arrive for phase *k+1* while the barrier is still finishing phase *k*, whose
 expected count is already zero. That is a precondition violation on `arrive()`, which is to
 say undefined behaviour, which is to say a deadlock on this implementation.

 The fix is one line of placement, and it is the same lesson as section 3 in a different
 costume: restock *after* the barrier, not inside it.

 ```cpp
void hydrogen(std::function<void()> r) {
    h_slots_.acquire();
    r();
    group_.arrive_and_wait(); // the phase has definitely advanced when this returns
    h_slots_.release(); // ...so this permit belongs to the next molecule
}

```

 With `std::barrier<> group_{3}` and no completion function, that version is correct,
 fifteen runs of twenty molecules each, zero bad groups. It is arguably the nicer design,
 since the quota and the group boundary become separate named objects instead of two
 integers and an `if`. It is also the version that shows you cannot reason about
 `std::barrier` by analogy; the completion step has rules, and "release the permits from the
 completion function" is exactly the intuitive thing that those rules forbid.

## 7. Interview follow-ups

**"The 'tidy' version, accounting under the lock, releaseHydrogen outside it, cut the
failure rate from 100% to about a third, and you called that 'the dangerous kind of
broken.' Why is a partial improvement more dangerous than a total failure?"** Because a bug
that fails 100% of the time gets caught immediately, by anyone who runs the code once. A bug
that produces long, visibly-correct stretches interrupted by occasional failures looks like
it's working under a quick glance or a light test suite, exactly the failure mode this
whole course keeps returning to (see [[013-store-buffering]]'s "not a rare timing accident"
framing for the same idea from a different angle). The counters being correct at every
instant is a real, verifiable fact that has nothing to do with whether the *emission order*
, the actual specification, is correct, and it's easy to mistake the first for evidence of
the second.

**"Why does the callback have to run inside the critical section here, when every other
lesson in this course says 'never call unknown code while holding a lock'?"** Because the
callback *is* the observable event the entire problem is about ordering, `releaseHydrogen()`
writing to the tape is the thing being specified, not a side effect of some other operation.
If it ran outside the lock, the bookkeeping (counters, reset) would be correctly serialized
while the actual visible output raced freely, exactly what the "tidy" broken version
demonstrates. The rule "don't call unknown code under a lock" is about avoiding deadlock and
unbounded critical sections; it's a default, not an absolute, and this question is the
worked exception, with the honest cost (callbacks must be short, non-blocking, and touch
nothing that leads back into `H2O`) named explicitly rather than glossed over.

**"You measured the barrier-based rewrite deadlocking 3/3 times after exactly two molecules.
Walk through why restocking permits inside the completion function is undefined behaviour,
not just a bad idea."** A barrier's completion function runs once, when the last participant
arrives for the current phase, but "the phase has advanced" and "the completion function has
finished running" aren't the same instant from every other thread's point of view. Releasing
next-phase semaphore permits *during* the completion step means a fast thread can acquire one
and call `arrive_and_wait()` for phase k+1 while the barrier itself is still mid-completion
for phase k, arriving for a phase whose expected-arrival count hasn't been reset yet is a
precondition violation on `arrive()`, i.e. undefined behaviour. The fix (restock *after* the
barrier returns, not from inside its completion function) works because by the time
`arrive_and_wait()` returns to the caller, the phase has unambiguously and completely
advanced for everyone.

**"Small machine, 1-2 cores. Does the thundering-herd cost from notify_all still matter
proportionally, or does it wash out?"** It still matters, and possibly worse proportionally,
with fewer cores, each of the 36 pointlessly-woken threads that immediately go back to sleep
still costs a real context switch, and those switches now compete more directly with the 2
threads that actually had useful work to do for the CPU's limited parallel capacity. The
absolute wasted-wakeup count doesn't change with core count (it's O(waiters) regardless), but
the relative cost of paying for those wakeups goes up when there's less spare capacity to
absorb them.

**"Production ops, this design has no timeout, and you noted three hydrogens with no
oxygen just hangs forever with no diagnostic. How would you actually detect this specific
starvation pattern in a live system, given the design can't detect it itself?"** Track
per-molecule completion latency and per-atom-kind queue depth as live metrics, a queue depth
for one atom kind that's growing while the other stays near zero (hydrogens piling up with no
oxygen arriving, or vice versa) is the production signature of exactly this supply imbalance,
visible externally well before it becomes an outright permanent stall. This is the same
"watch the trend, not just the binary hung/not-hung state" principle
[[008-latch-barrier]]'s reading applies to its own straggler-detection follow-up.
