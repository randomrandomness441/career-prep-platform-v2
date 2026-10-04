## 1. Reframe the problem

Two threads, each with its own loop, and you need their prints to interleave perfectly.

The instinct is to make the threads take turns. You cannot. **You do not schedule threads,
the OS does, and it will happily run one loop to completion before the other one starts.**
The only lever you have is how long a thread waits before it does its next print.

So rewrite the problem: *both loops run flat out; the one whose turn it is not goes to
sleep.* That turns "alternate two threads" into two much smaller questions:

1. **What is the turn?** One `bool foo_turn`, shared, starting `true`. That single bit is
 the entire protocol. `foo` may print only when it is `true`, `bar` only when it is
 `false`, and each flips it after printing. Since exactly one of the two predicates is
 satisfiable at any moment, only one thread can be runnable at a time, that *is* the
 alternation.
2. **How does a thread sleep on a bool?** That is what `std::condition_variable` is: a
 waiting room attached to a mutex-protected piece of state. `cv.wait(lk, pred)` means
 "unlock, sleep, and don't come back until `pred` is true".

Two facts fall out of that framing, and they are the whole lesson of this question.

**`notify_one` is correct here, and it is correct for a specific reason.** There are two
threads and two predicates, and they are exact opposites: `foo_turn` and `!foo_turn`.
Whenever `foo` flips the flag, the only thread that can possibly be asleep on this
condition variable is `bar`, and `bar`'s predicate has just become true. So the single
thread `notify_one` wakes is guaranteed to be a thread that can make progress. There is no
one else to wake. The rule to remember: **`notify_one` is safe when every waiter is
interchangeable, or when there can only ever be one.** Both hold here. Neither holds in the
FizzBuzz question two problems from now, and that is what breaks it.

**The predicate is not decoration.** `cv.wait(lk)`, the one-argument form, means "wake me
when someone notifies". `cv.wait(lk, pred)` means "wake me when `pred` is true". Those are
different requests, and only the second one is the thing you actually want. A condition
variable is allowed to wake a sleeper for no reason at all (a *spurious wakeup*), and it is
allowed to be notified by code that has nothing to do with you. Waking up is not evidence.
The flag is the evidence, so you must re-read the flag, under the lock, every time you wake.
The predicate form does exactly that, it is a `while` loop, not an `if`:

```cpp
while (!pred()) cv.wait(lk); // this is what cv.wait(lk, pred) expands to
```

Write it as `if`, and one unexplained wakeup prints out of turn.

## 3. The broken version, first

### Attempt one: no synchronisation

```cpp
class FooBar {
    int n;
public:
    void foo(std::function<void()> printFoo) { for (int i = 0; i < n; ++i) printFoo(); }
    void bar(std::function<void()> printBar) { for (int i = 0; i < n; ++i) printBar(); }
};
```

Run with `n = 5`, `bar`'s thread started first, ten times in a row:

```
got barbarbarbarbarfoofoofoofoofoo
expected foobarfoobarfoobarfoobarfoobar
```

Ten runs, ten identical outputs. Not "sometimes wrong", the machine has a preferred
schedule and it takes it every time. `bar`'s thread was created a few microseconds earlier,
so it finished its entire loop before `foo`'s thread got going.

The harness agrees, with `n = 100`:

```
failed 12/12 runs

trial 0: output diverges at character 0
 got barbarbarbarbarbarbarbarbarbar
 expected foobarfoobarfoobarfoobarfoobar
 (600 characters produced, 600 expected)
```

Note the last line: **600 characters produced, 600 expected.** Every print happened, the
right number of times. Nothing is missing and nothing raced on the string, the harness
locks around its own buffer. The count is perfect and the answer is worthless. Ordering is
a separate property from mutual exclusion, and no amount of the second gives you the first.

### Attempt two: a condition variable, but `wait` without the predicate

Now the version that is 95% right, which is the dangerous kind:

```cpp
void foo(std::function<void()> printFoo) {
    for (int i = 0; i < n; ++i) {
        std::unique_lock<std::mutex> lk(m);
        if (!foo_turn) cv.wait(lk); // woken == "my turn", allegedly
        printFoo();
        foo_turn = false;
        lk.unlock();
        cv.notify_one();
    }
}
```

`bar` is the mirror image. This passes. Two hundred runs on this machine:

```
failed 0 / 200
```

Two hundred green runs and the code is still wrong. It survives only because of an
accident of this problem's shape: there are exactly two threads, and the *only* code that
ever touches this condition variable is the code you are looking at. So in practice every
wakeup really was meant for the thread that got it.

Take that accident away and watch it fall over. Here is the same code with one extra thread
that calls `cv.notify_all()` on the same condition variable, which is precisely what a
spurious wakeup looks like from inside `foo`: you were woken, and the flag says it is not
your turn. `n = 2000`, twenty runs:

```
first divergence at char 12
 got ...foobarfoobarbarbarfoobarfoobar
 expected ...foobarfoobarfoobarfoobarfoobar

first divergence at char 0
 got ...barbarfoobarfoobarfoobarfoobar
 expected ...foobarfoobarfoobarfoobarfoobar

failed 20 / 20
```

`barbar`. `bar` woke up, did not re-read `foo_turn`, and printed anyway.

Change one line, `if (!foo_turn) cv.wait(lk);` becomes
`cv.wait(lk, [&]{ return foo_turn; });`, and run the identical notification storm:

```
predicate version failed 0 / 20 under the same notification storm
```

That is the case for the predicate form in one pair of numbers: 20/20 broken versus 0/20.
The bare `wait` is not a style choice, and you cannot test your way to confidence in it,
because the event that breaks it is one your hardware may simply never produce. The
standard permits it; your laptop declining to demonstrate it proves nothing.

## 6. Where this solution fails

- **The flag `foo_turn` is only meaningful under the mutex.** Reading it outside the lock,
 say, an `if (foo_turn)` fast path before locking, is a data race, and the value you read
 can be stale by the time you act on it. The mutex is not protecting the *bool*; it is
 protecting the *decision* you make from that bool.
- **Notifying while still holding the lock is legal but wasteful.** The woken thread
 immediately tries to lock the mutex you are still holding, so it goes straight back to
 sleep and has to be woken a second time. Some implementations optimise this away; not all
 do. `lk.unlock(); cv.notify_one();` avoids the question.
- **A throwing callback deadlocks it permanently.** If `printFoo()` throws, the loop
 unwinds with `foo_turn` still `true`. `bar` is asleep waiting for `false`, nobody will
 ever set it, and `bar`'s thread never returns, so the join in the caller hangs forever
 and the process cannot exit. The fix is to flip the flag and notify from a scope guard, or
 to record the exception and set the flag anyway so the other side can wake up and rethrow.
- **`notify_one` stops being correct the moment a third waiter exists.** Add a `baz()` and
 the condition variable now has two sleepers with *different* predicates. `notify_one` may
 wake the one whose predicate is still false; that thread rechecks, goes back to sleep, and
 the notification is gone, nobody else was told. Every thread now sleeps forever. That is
 the lost-wakeup deadlock, and it is the whole subject of the FizzBuzz question. The
 narrowness of "one waiter, therefore `notify_one`" is easy to miss when you copy this code
 into a bigger class.
- **One condition variable serialises everything through one mutex.** Every round trip is
 lock, sleep, wake, re-lock. For strict two-way ping-pong, two `std::binary_semaphore`s
 express the same thing with no mutex and no predicate, `foo` acquires `sem_foo` and
 releases `sem_bar`, `bar` does the reverse, and semaphores are dramatically cheaper on a
 tight handoff. Measured here, 200,000 round trips, `-O2`, three repeats: `binary_semaphore`
 **247–341 ns** per round trip, mutex + condition variable **22.6–26.7 µs**. Roughly 80x,
 because every condition-variable handoff makes both threads collide on the same mutex and
 the woken thread must re-acquire it. The condition variable earns its keep when the waiting
 condition is richer than "a token arrived"; here it is not.
- **`n <= 0` must fall out correctly.** Both loops run zero times and both calls return,
 fine. But an implementation that waits *before* checking the loop bound, or that primes a
 semaphore in the constructor and never consumes it, leaves a thread asleep or a token
 dangling. Trace `n = 0` through your version explicitly.
- **The object is single-use in spirit, and nothing enforces it.** Calling `foo()` twice on
 the same `FooBar` from two different threads compiles, and silently produces `2n` foos
 fighting over one turn flag. The class has no way to say "exactly one caller each".

## 7. Interview follow-ups

**"You measured semaphores beating a mutex+condition-variable handoff by ~80x. Why is the
gap so large for this specific problem, when this course has also shown atomics losing to
mutexes elsewhere?"** The gap is large here specifically because the "condition" being waited
on is as simple as it can possibly be, "a token arrived", with no actual shared data to
inspect. A condition variable's overhead (lock, sleep, wake, re-acquire the same mutex both
threads collide on) buys you the ability to wait on an arbitrarily rich predicate; when the
predicate really is just a token, you're paying that overhead for nothing. Compare this to
[[030-atomics-vs-mutexes]], where an atomic loses under *contention* on a shared counter,
that's a different comparison (many threads racing one value) than this one (two threads
handing off a turn with no real contention at all).

**"You said notify_one stops being correct the moment a third waiter exists, walk through
exactly why, using this problem's own two-thread case as the contrast."** With exactly two
threads and exactly two complementary predicates (`foo_turn` and `!foo_turn`), whichever
thread `notify_one` wakes is *guaranteed* to be the one whose predicate just became true,
there's no other candidate. Add a third waiter with its own, different predicate, and
`notify_one` might wake that third thread instead; it rechecks, finds its own condition still
false, and goes back to sleep, and the notification that was meant for someone else is now
gone, with nobody told. This is the exact lost-wakeup mechanism [[019-fizzbuzz-multithreaded]]
is built entirely around; the FooBar pattern here is the last shape where `notify_one` is
safe, not a general recipe.

**"A throwing printFoo() deadlocks the whole thing permanently, how would you actually fix
this without changing the class's public interface?"** Flip the flag and notify from an RAII
scope guard rather than from the normal control-flow path, so it runs regardless of whether
the callback returned normally or threw, the same "release what you hold, on every exit
path, including exceptions" discipline [[005-scoped-lock]]'s `lock_guard` already applies to
mutexes, generalized here to a condition-variable turn flag. The alternative (catch the
exception, still flip the flag, then rethrow) works too but has to be done at every call
site inside the loop rather than once in a guard's destructor.

**"Small machine, 1-2 cores. Does the semaphore-vs-condvar performance gap still matter, or
does it wash out under scheduling overhead?"** The absolute magnitude of both numbers would
likely shift (more context-switch pressure with fewer cores to hide it behind), but the
*relative* gap should persist, because it comes from a structural difference (how many
kernel round trips and mutex re-acquisitions each primitive needs per handoff), not from
raw core count. Fewer cores means both approaches get slower; it doesn't remove the extra
work the condition-variable version is doing per handoff that the semaphore version isn't.

**"n=0 needs to fall out correctly, you said, what's the actual failure if someone gets this
wrong, concretely?"** An implementation that waits (checks its turn) *before* checking the
loop bound, rather than checking the bound first, ends up parking a thread on a condition
that's never going to be signaled, for `n=0` neither loop ever runs, so nobody ever flips
the turn flag or releases the corresponding semaphore, and a thread that waited first finds
itself asleep forever with no join ever completing. Tracing `n=0` explicitly through your own
control flow, does the loop condition get checked before any wait, unconditionally, is the
concrete thing to verify, not just "handle the edge case" as an abstract instruction.
