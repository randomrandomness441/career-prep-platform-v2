## 1. Reframe the problem

"Book a seat" sounds like a single action, but it's really two: check whether the seat is
free, then claim it. Written as two separate steps, an `if`, then an assignment, there's a
gap between them where another thread can run its own check before yours has claimed
anything, and both of you walk away believing you won. This is the check-then-act pattern
this course keeps returning to (a boolean flag, a free-list slot, a histogram bucket), and
the fix is always the same shape: turn "check, then act" into one operation neither thread
can split. Here the natural tool is `compare_exchange` on a per-seat atomic, which is
exactly what "optimistic concurrency control" means in practice, attempt the claim
optimistically, and let the atomic itself tell you whether you actually won.

## 3. The broken version, first

```cpp
if (owner_[seat] == -1) {
    owner_[seat] = user_id;
    return true;
}
```

**Why it looks right:** it's the direct translation of the English sentence "if the seat is
free, take it", check, then act, exactly the order you'd say it out loud. For a single
caller, or for callers that happen never to collide in testing, it's indistinguishable from
correct.

Run against the correctness suite alone, it **passes**, 400 users contending for 20 seats,
every seat won by exactly one caller, every run:

```
solution.cpp -> CLEAN
boilerplate.cpp -> CLEAN (correctness stage only)
```

Only the full sweep, under ThreadSanitizer, reveals the actual defect:

```
WARNING: ThreadSanitizer: data race
 Read of size 4 by thread T21 ... previous write of size 4 by thread T1
```

A genuine unguarded read/write race on `owner_[seat]`, the boilerplate is
`CORRECT_BUT_RACY`, this platform's middle verdict tier, precisely for cases like this one:
a race that happens not to produce a wrong answer on the specific runs a correctness suite
exercises is still a race, and still undefined behaviour by the language's own rules. See
[[036-parallel-graph-bfs]] for another worked example of the same gap between "passes the
tests" and "actually correct."

The fix is one atomic compare-and-swap per attempt:

```cpp
int expected = -1;
return owner_[seat].compare_exchange_strong(expected, user_id, ...);
```

Full sweep: `CLEAN` across compile, 12/12 correctness, 20 TSan runs, 150 shaken-stress runs.

## 6. Where this solution fails

- **A user who calls `reserve` twice, a double-click, a retried network request, gets
 billed as two different reservation attempts, with no protection against acting on the
 second one.** If the first call already succeeded, the second correctly returns `false`
 (the seat's owner no longer matches `-1`), which happens to be safe here, but a richer
 system (charging a card, sending a confirmation email) needs an actual idempotency key per
 *attempt*, not per seat, so a retried request is recognized as "the same request" rather
 than merely failing to double-book by coincidence.
- **This is optimistic concurrency control, and it's the right choice specifically because
 contention on any one seat is brief**, the "critical section" is one CAS, not a stretch of
 work held under a lock. A system where the "reservation" step does real, slow work (holding
 a seat provisionally while collecting payment) is a different problem: pessimistic locking
 (hold a lock or a real DB row-lock for the full duration) is usually the better fit there,
 precisely because you don't want a hundred threads spinning through failed CAS attempts
 against a seat that's genuinely busy for seconds, not nanoseconds.
- **Booking multiple seats in one transaction (a party of four wants adjacent seats) isn't
 covered by this per-seat design at all.** Each `reserve` call is independent and atomic on
 its own, but nothing here makes a *group* of reservations atomic together, a caller could
 win seats 5 and 6 individually while another caller wins 7, leaving the first caller with
 an incomplete booking they now have to unwind. That's the guide's "lock seats in ascending
 order" deadlock-avoidance advice answering a different question (pessimistic locking of
 multiple seats together) than the one this design solves.
- **No fairness guarantee under sustained contention.** If a seat is contested by many
 callers simultaneously and repeatedly (not this test's shape, one attempt per caller,
 but a realistic "keep retrying until you get any seat" client), `compare_exchange`
 guarantees at most one winner per attempt, not that attempts are served in any particular
 order.

## 7. Interview follow-ups

**"When would you actually reach for pessimistic locking (SELECT FOR UPDATE, a real mutex)
over this optimistic approach?"** When contention is high *and* the work done while "holding"
the resource is non-trivial, the guide's own framing: pessimistic locking avoids wasted
retries when collisions are common and expensive to redo, optimistic concurrency avoids
holding a lock during idle time (a user staring at a seat map deciding) when collisions are
rare. This exercise's shape, one atomic CAS, nothing else, is close to the ideal case for
optimistic: the "work" being contended over is as cheap as the synchronization itself.

**"A user's reservation request times out on their end and their client retries, what
actually happens, and is it safe?"** With this design: the retry calls `reserve` again for
the same seat. If the original request actually succeeded server-side (the timeout was only
on the client's view of the response), the retry correctly gets `false`, no double booking,
because the atomic state is the source of truth, not whether any particular caller believes
it won. The unsafe case is elsewhere: if the *seat* the client meant to retry got released or
reassigned in between (not possible in this simple model with no seat-release operation, but
realistic in a fuller system), an idempotency key tied to the original *request*, not the
seat, is what correctly recognizes "this is the same booking attempt" regardless of what
state the seat is in by the time the retry arrives.

**"What's the actual cost difference between this and a mutex-per-seat pessimistic design,
under the contention this test uses?"** The mutex version pays for lock/unlock (and,
under contention, potential OS-level arbitration) on every attempt, win or lose; the CAS
version pays for one atomic instruction, and a failed CAS is cheaper than a failed lock
acquisition in essentially every real implementation, see [[030-atomics-vs-mutexes]] for
measured numbers on that exact tradeoff in isolation. The gap narrows or reverses if the
"critical section" stops being trivial, which is exactly the condition under which
pessimistic locking becomes the better choice instead.
