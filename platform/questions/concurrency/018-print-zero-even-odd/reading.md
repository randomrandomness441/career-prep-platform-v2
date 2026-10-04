## 1. Reframe the problem

Three threads, one output sequence, and the sequence is not "each thread in turn". It is
`0 1 0 2 0 3 0 4 0 5`. Look at whose turn it is at each step:

```
0 1 0 2 0 3 0 4 0 5
z o z e z o z e z o
```

`zero` runs every other step. Between them the turn ping-pongs between `odd` and `even`.
So this is not a three-way rotation, it is a **hub and two spokes**. `zero` is the hub; it
runs, and then it must decide which spoke gets to run next. `odd` and `even` each run, and
then hand control straight back to the hub.

That reframing gives you the answer almost for free:

- Every participant needs somewhere to sleep. Three participants, three sleeping places.
 One `std::binary_semaphore` each. `zero`'s starts with a token (`{1}`, the sequence
 begins with a `0`); the other two start empty (`{0}`).
- `zero` is a **router**. On iteration `i` it prints the `0` that precedes the number `i`,
 so `i % 2` tells it which spoke to open. That single `if` is the entire ordering logic of
 the program.
- `odd` and `even` are trivial: wait at my gate, print my number, reopen the hub's gate.

Now the property this question exists to teach. Look at `sem_zero`. It is **acquired by the
`zero` thread and released by the `odd` and `even` threads.** Nobody "holds" it. A semaphore
is a bowl of tokens with no record of who put them there:

- `acquire()`, take a token; if the bowl is empty, sleep until someone adds one.
- `release()`, add a token, waking one sleeper.

**A semaphore has no owner.** That is not a detail, it is the reason this problem is a
semaphore problem. A `std::mutex` is the opposite: it is *owned* by the thread that locked
it, and only that thread may unlock it. `sem_zero` is a message from `odd` to `zero` saying
"your turn", a message from one thread to a different thread. A mutex cannot carry that,
because the only thread allowed to release a mutex is the one that already has it, and that
thread is precisely the one that does not need telling.

If you catch yourself reaching for "three mutexes, each thread locks its own", stop: the
thread that must unlock is never the thread that locked. What you are actually building is
a signal, and a signal needs an ownerless primitive, a semaphore, or a condition variable,
or an atomic flag with `wait`/`notify`.

## 3. The broken version, first

The plausible wrong version is two semaphores instead of three: one gate for `zero`, one
gate meaning "a number may now be printed", shared by `odd` and `even`.

```cpp
std::binary_semaphore sem_zero{1};
std::binary_semaphore sem_num{0};

void zero(auto p) { for (int i = 1; i <= n; ++i) { sem_zero.acquire(); p(0); sem_num.release(); } }
void even(auto p) { for (int i = 2; i <= n; i += 2) { sem_num.acquire(); p(i); sem_zero.release(); } }
void odd (auto p) { for (int i = 1; i <= n; i += 2) { sem_num.acquire(); p(i); sem_zero.release(); } }
```

The token accounting is perfect. `zero` releases exactly `n` tokens onto `sem_num`, and
`odd` plus `even` acquire exactly `n` between them. Nothing leaks, nothing deadlocks, every
value gets printed exactly once. Thirty runs at `n = 10`, none of them hung, and none of
them was right:

```
got 0 1 0 3 0 5 0 7 0 9 0 2 0 4 0 6 0 8 0 10
expected 0 1 0 2 0 3 0 4 0 5 0 6 0 7 0 8 0 9 0 10

got 0 2 0 4 0 6 0 8 0 10 0 1 0 3 0 5 0 7 0 9
expected 0 1 0 2 0 3 0 4 0 5 0 6 0 7 0 8 0 9 0 10

pass=0 wrong=30 hang=0
```

And through the harness at `n = 51`:

```
failed 12/12 runs

trial 0: sequence diverges at position 1
 got 0 2 0 4 0 6 0 8 0 10 0 1 0 3 0 5 0 7
 expected 0 1 0 2 0 3 0 4 0 5 0 6 0 7 0 8 0 9
```

Read the first failure carefully: it printed **every odd number, then every even number**.
One thread woke up at `sem_num` and then kept winning the gate, over and over, until its
loop was finished. That is not bad luck, a woken thread is already hot in cache and already
scheduled, so it beats the sleeping one to the next token almost every time. The second
example shows the other thread winning instead. Both are stable for the whole run.

The bug in one sentence: **the code never decides who wakes up.** `release()` on a semaphore
with two sleepers wakes one of them, and which one is unspecified, the standard says
nothing, and libc++ hands the choice to the kernel's wait queue. The fix is not a better
`release`; you cannot address a semaphore. The fix is to stop putting two different waiters
in the same waiting room. One gate per thread means every `release()` names its recipient.

This is worth generalising, because it comes back in every signalling problem: **a wakeup
that can go to more than one place is a wakeup you have not specified.** Either all your
waiters are interchangeable, as in a thread pool, where any worker will do, or each one
needs its own place to wait.

### And a mutex really cannot do this

The claim above is worth checking rather than believing. Here is `main` locking a
`std::mutex` and a *different* thread unlocking it, on this machine, Apple clang 21, libc++:

```
main: locked m
other: calling m.unlock() on a mutex I never locked
other: unlock returned
main: trying to lock m again...
main: got it back
semaphore: acquired a token released by another thread - fine
exit=0
```

It worked. Three runs, all identical, and **ThreadSanitizer reported nothing.** That is the
trap, not a reprieve: unlocking a mutex you do not own is undefined behaviour, and this
implementation happens to be a thin wrapper over a fast `pthread_mutex_t` that does not
store an owner, so nothing catches you. Ask for a mutex that does check, and the same call
is rejected:

```
main: locked
other thread's pthread_mutex_unlock returned 1 (Operation not permitted)
owning thread's pthread_mutex_unlock returned 0
```

`EPERM`, operation not permitted, from the exact call that silently "succeeded" a moment
ago. "It ran fine" is not evidence that a mutex can be used as a signal. A semaphore *is
specified* to allow the cross-thread handoff; a mutex merely fails to notice.

## 6. Where this solution fails

- **A throwing callback deadlocks it permanently.** If `printNumber(0)` throws inside
 `zero()`, no gate is opened, and `odd` and `even` sleep forever. Their threads never
 return, so joining them hangs and the process cannot exit, no diagnostic, no timeout, no
 stack to look at. This is the single most common real-world failure of the pattern, and it
 is not hypothetical: the callback is user code you did not write. Opening the next gate
 from a scope guard survives the throw but then prints the wrong sequence, so the honest
 fix is a "poisoned" flag every waiter checks after `acquire()`.
- **Ownerless is exactly what makes it unauditable.** The property that solves the problem
 also removes every safety net a mutex gives you. There is no RAII wrapper, so an early
 `return` or an exception between `acquire()` and `release()` leaks a token permanently.
 TSan's lock-order checker does not model semaphores, so it cannot tell you that your gates
 form a cycle. `-Wthread-safety` annotations do not apply. You are on your own, and the
 failure is a silent hang.
- **Releasing a `binary_semaphore` that already holds its token is undefined behaviour.**
 Not an exception, not a saturating counter, UB, with `max() == 1`. Any restructuring that
 lets two threads release the same gate before it is acquired is a bug you will not see.
 If that is a real possibility, use `std::counting_semaphore<K>` with an honest bound.
- **No timeout and no cancellation.** `acquire()` waits forever. If the `zero` thread has
 died, `odd` and `even` wait for the rest of the process's life. `try_acquire_for()` gives
 you a timeout but then you have to decide what a timeout *means* mid-sequence, and there
 is no good answer, the state is half-way through a handoff.
- **It does not scale past a fixed, known set of participants.** Three roles, three
 semaphores, hard-coded routing. Add "print a `#` before every multiple of 7" and you edit
 the class, add a member, and extend the `if` in `zero()`. Anything with a variable number
 of participants wants one mutex plus a shared counter and a condition variable, which is
 the shape the FizzBuzz question uses, and which brings its own problem.
- **Correct but wildly slower than not doing it at all.** Every number costs two thread
 handoffs. Measured here, `n = 100,000`, `-O2`: **~550–620 ns per number** for the
 three-thread version versus **~0.9–1.0 ns** for the identical output produced by one plain
 loop. That is roughly 600x, and it is inherent, the work per step is a `push_back`, and
 a context switch costs hundreds of times more than that. This question is a synchronisation
 exercise, not a design. If you ever see this shape in production code, the question to ask
 is why there are three threads at all.
- **`n = 0` and `n < 0` are fine but only by accident.** Every loop runs zero times and all
 three calls return immediately, leaving one unconsumed token in `sem_zero`. That is
 harmless here because the object is then destroyed, but it is worth noticing that the
 class has no idea it is finished, nothing in it distinguishes "done" from "not started".

## 7. Interview follow-ups

**"You demonstrated a mutex genuinely being unlocked from a different thread and getting
EPERM in return, why does a semaphore not have this problem, structurally?"** A
`std::mutex` has an *owner*, the thread that locked it is the only thread the implementation
permits to unlock it, enforced by the platform's own mutex primitive (as the `EPERM` from
`pthread_mutex_unlock` shows directly). A semaphore has no such concept: `acquire()` and
`release()` are symmetric operations on a shared count, callable from any thread regardless
of which thread called the other. That ownerless property is exactly what a cross-thread
handoff needs, and exactly what a mutex is specified to refuse.

**"Measured: ~600x slower than a plain loop for the same output. If a candidate proposed this
three-thread design for a real logging pipeline, what would you tell them?"** That the
600x isn't a rounding error or an implementation quirk to optimize away, it's inherent to
doing two thread handoffs (context switches) per unit of output whose actual work
(`push_back`) costs nanoseconds. No amount of tuning this design closes that gap; the fix is
recognizing three threads were never the right tool for interleaving output in a fixed
pattern, and asking why the design reaches for concurrency at all when the "hard part" here
is turn-taking, not parallel work.

**"A throwing printNumber(0) deadlocks odd and even permanently, same as 017's FooBar case,
does the fix generalize the same way here?"** Mostly, with one added wrinkle noted directly:
opening the next gate from a scope guard survives the throw (so nobody hangs), but then the
*sequence* printed is wrong, because a gate opened despite the callback failing doesn't mean
the right thing actually got printed. The honest fix needs a "poisoned" flag every waiter
checks immediately after `acquire()`, so a thread that wakes up due to the recovery path can
tell the difference between "my turn genuinely arrived" and "someone upstream failed and is
just trying not to hang everyone else."

**"Small machine, 1-2 cores. Three threads handing off through semaphores, does context-
switch cost change the 600x number meaningfully?"** The absolute per-number cost would likely
grow (fewer cores means more forced context switches as the OS time-slices three threads
across one or two cores rather than letting them each have dedicated hardware), but the
*qualitative* conclusion doesn't change: the work being interleaved is still nanoseconds, the
handoff mechanism is still hundreds of nanoseconds at best, and that ratio is what makes this
a synchronization exercise rather than a real design regardless of core count.

**"How would you extend this to a variable number of participants (not just zero/even/odd),
'print a # before every multiple of 7,' say, and why is that the wrong direction to push
this specific design?"** Adding a fourth semaphore and a fourth `if` branch in `zero()`
technically works but doesn't scale, every new participant means editing the class,
adding a member, and extending the routing logic by hand. The reading names the actual right
tool for a variable, extensible number of turn-takers: one mutex, a shared counter, and a
condition variable with a predicate, the shape [[019-fizzbuzz-multithreaded]] uses, which
trades this design's raw speed for generality, and which brings its own distinct failure
mode (`notify_one` losing wakeups among waiters with different predicates) that this
three-semaphore design never has to worry about.
