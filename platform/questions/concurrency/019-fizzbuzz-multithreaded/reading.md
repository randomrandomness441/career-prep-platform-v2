## 1. Reframe the problem

Four threads, one shared counter, and each thread only cares about *some* of the values,
`fizz` wants multiples of 3, `buzz` wants multiples of 5, `fizzbuzz` wants multiples of 15,
`number` wants everything else. The naive instinct is "one condition variable, one predicate
per thread", that's [[017-print-foobar]]'s two-thread trick with two more threads bolted on.

It doesn't scale, and the reason is the whole lesson: **`notify_one` wakes *a* thread, not
*the right* thread.** With two threads and two opposite predicates, whichever one you don't
wake is guaranteed to be the one whose turn it is, that's why FooBar's `notify_one` works.
With four threads and four *different* predicates, the thread `notify_one` happens to wake
can easily be one whose predicate is still false. It rechecks, finds nothing to do, and goes
back to sleep, and the notification is gone. Nobody else was told. That's a **lost wakeup**,
and with four asymmetric waiters on one condition variable it is not a rare accident, it is
the default outcome.

The fix is not a smarter predicate, it's `notify_all`. Wake every thread on every value.
Three of them recheck their predicate, find it false, and go straight back to sleep. That's
wasteful compared to `notify_one`, but it's the only way to guarantee the *one* thread whose
predicate just became true actually gets to check.

## 3. The broken version, first

The broken version is a one-word diff from the correct one. Same mutex, same condition
variable, same predicate per thread, same "unlock before notifying" discipline, the kind of
code that passes review because every line, read alone, is standard practice:

```cpp
cv.notify_one(); // looks efficient, wake exactly the thread that can proceed
```

**Why it looks right:** with `n = 15` and the thread that owns value 1 (`number`) about to
run, `notify_one` "sounds" targeted, why wake three threads that have nothing to do? It's
the same reasoning that makes `notify_one` the *correct* choice in FooBar, so it's easy to
carry the instinct over without noticing the predicates are no longer complementary.

Running it, 12 trials, `n = 45`, every one hangs:

```
$ ./bad_test
(no output, killed after 3s, exit 142/SIGALRM)
```

No partial output, no crash, no TSan complaint, the process is just gone. Here's why, traced
through one interleaving: `fizzbuzz`'s thread happens to be first to grab the lock at
`current = 1`. Its predicate (`v % 15 == 0`) is false, so it calls `cv.wait`, which atomically
unlocks and sleeps. `number`'s thread locks next, sees `current = 1` satisfies its predicate,
prints `1`, sets `current = 2`, unlocks, and calls `notify_one`. That wakes *some* sleeping
thread, say `fizz`, whose predicate (`2 % 3 == 0`) is also false. `fizz` goes straight back
to sleep. `fizzbuzz` and `buzz`, both still asleep and still correctly waiting for their turn,
were never told anything changed. Eventually every thread but the one that just ran is asleep,
and there is no thread left to advance the counter past their turn. Four threads, one dead
condition variable, zero remaining callers to wake anyone.

Change the one identifier, `cv.notify_all()`, and the same 12 trials at `n = 45`:

```
$ ./sol_test
10 trials x n=45 in order, and n=0 returns all four threads
```

Every wakeup now reaches every sleeper, each rechecks its own predicate under the lock, and
exactly one of them ever finds it true. The other three pay a wasted wake-check-sleep cycle,
cheap compared to a hang.

## 6. Where this solution fails

- **`notify_one` is not a drop-in optimization here, ever.** It looks like a targeted wake
 when in fact which thread it targets is chosen by the OS scheduler, not by which predicate
 is true. This is the deadlock this whole question exists to teach, see section 3.
- **A throwing `emit` callback strands three threads permanently.** If `printFizz()` (or any
 of the four) throws, the loop unwinds without calling `notify_all()` for the value it never
 finished. The other three threads are asleep waiting for `current` to advance past a value
 that now never will. Production code needs a `catch` that still notifies (or sets a
 "failed" flag the predicate also checks) before rethrowing.
- **Reading `current` outside the lock is a race**, including in test code, the harness's
 own `record()` callback is safe only because it takes its own separate mutex; if you added
 a fast-path check of `current` before locking `m`, that reads memory another thread may be
 writing.
- **This is intrinsically a `notify_all` design, and that has a cost that gets worse with
 more waiters.** Every value wakes all four threads to let one of them proceed, O(waiters)
 wakeups per useful print. FizzBuzz caps at four, so it doesn't matter here, but the same
 shape with, say, 100 threads each waiting on a distinct shard would turn every notification
 into a 100-thread stampede for one winner. See [[025-thundering-herd]] for exactly that
 failure mode isolated and measured.
- **`n = 0` is not a trivial case to skip.** All four threads must still return with zero
 prints. A solution that checks the loop bound *after* the first `cv.wait` call rather than
 before it will leave every thread asleep forever on an empty sequence.

## 7. Interview follow-ups

**"Why not just give each thread its own condition variable?"**
You can, and it's a legitimate fix, four `cv`s, each thread `notify`s the specific one that
owns the next value. That turns every notification back into a `notify_one` on a
single-waiter `cv`, which is cheap and race-free by the same argument as FooBar. The tradeoff
is bookkeeping: `run()` would need to look up which of four condition variables corresponds
to "the next value's owner," which is exactly the dispatch problem `notify_all` sidesteps by
brute force. For four threads it's a wash; the guide's point in choosing `notify_all` is that
it generalizes to an *unknown* number of waiters without new code.

**"What if `n` is huge, does `notify_all`'s waste start to matter?"**
Per value you pay three wasted wake-check-sleep cycles (13 for a hypothetical 16th thread
design). That cost is O(waiters), not O(n), it doesn't compound across values, it's a flat
per-notification tax. At `n = 10^8` you'd do 10^8 notifications × 3 wasted wakeups each; the
fix isn't a smarter condition variable, it's restructuring so each thread claims a *range* of
values up front (partition the sequence 4 ways) instead of contending value-by-value, trading
strict in-order interleaving for throughput, which changes the problem's contract.

**"A fifth thread joins mid-run and calls `fizz()` a second time, what happens?**
Two threads now share the `fizz` predicate. Both wake on every `notify_all`, both may see
`v % 3 == 0 && v % 5 != 0` true, and both race to print, the mutex serializes the increment
so you won't corrupt `current`, but you can get two `fizz` prints in a row for one value, or a
skipped one, depending on which thread's `emit` runs first. The class was written assuming
exactly one caller per method; nothing enforces that, and the failure is silent, not a crash.

**"How would you detect this hang in production instead of after a 3-second test timeout?"**
A watchdog thread that checks `current` periodically and alerts if it hasn't advanced in,
say, 100ms while callers are known to still be joined, cheap and effective for exactly this
"stuck, not crashed" failure mode. `./check`'s own stress stage does the same thing at test
scale: it kills any run over 12s and reports it as a hang rather than waiting forever.
