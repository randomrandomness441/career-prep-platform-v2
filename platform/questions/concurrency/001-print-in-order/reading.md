## 1. Reframe the problem

Three threads call `first()`, `second()`, `third()`. The OS starts them in whatever order
it likes and you have no say in it.

So stop trying to order the threads. **You cannot control when a thread starts, only how
long it waits.** Rewritten: *let all three start whenever; make the wrong ones block until
the right one has finished.* Ordering becomes a waiting problem, and waiting is something
C++ gives you tools for.

## 2. The tools

### Semaphore, a bowl of tokens

Two operations:

- `acquire()`, take a token. If the bowl is empty, **sleep** until someone adds one.
- `release()`, add a token, waking one sleeper.

```cpp
#include <semaphore>

std::binary_semaphore gate{0}; // 0 = bowl starts empty = gate shut

// thread B
gate.acquire(); // sleeps here; bowl is empty
std::puts("second");

// thread A
std::puts("first");
gate.release(); // drops a token in; B wakes and proceeds
```

Order is now guaranteed regardless of which thread the OS starts first. If B runs first it
sleeps at `acquire()`. If A runs first the token is already waiting and B walks through.

`binary_semaphore` holds at most 1 token; `counting_semaphore<N>` holds up to N.

**The property that matters here: a semaphore has no owner.** Thread A can release a token
that thread B takes. A `std::mutex` cannot do this, it must be unlocked by the same thread
that locked it. Signalling *between* threads requires that ownerless property, which is why
"just use a mutex" does not work for this problem.

Two gates chain three threads:

```cpp
std::binary_semaphore gate2{0}, gate3{0};
// first(): print, then gate2.release()
// second(): gate2.acquire(), print, then gate3.release()
// third(): gate3.acquire(), print
```

### Condition variable, sleep until a condition holds

```cpp
std::mutex m;
std::condition_variable cv;
int turn = 1; // guarded by m

void go(int me) {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [&]{ return turn == me; }); // sleep until it is my turn
    print();
    ++turn;
    cv.notify_all();
}
```

`cv.wait(lk, pred)` atomically: checks `pred`; if false, unlocks the mutex and sleeps; on
waking, re-locks and re-checks.

**Always use the predicate form.** A thread can wake up for no reason at all, a *spurious
wakeup*, permitted by the standard because it lets implementations be much faster. Waking
does not mean your condition became true.

## 3. The broken version, first

Almost everyone reaches for a flag:

```cpp
class Foo {
    bool done1 = false, done2 = false;
public:
    void first (auto p) { p(); done1 = true; }
    void second(auto p) { while (!done1) {} p(); done2 = true; }
    void third (auto p) { while (!done2) {} p(); }
};
```

Compiled at `-O1` this produced:

```
trial 0 produced "thirdfirstsecond", expected "firstsecondthird"
```

The ordering did not merely fail under bad luck, it failed *completely*. `done2` is never
modified anywhere the compiler can see inside `third()`, so `while (!done2) {}` is either
an infinite loop or dead code, and an infinite loop with no side effects is undefined
behaviour. Clang deleted the loop. `third()` ran immediately.

Patch it with `volatile` to stop the deletion and the output becomes correct, and the
code is still broken:

```
[1/4] compile ok
[2/4] correctness 12/12 runs ok
[3/4] ThreadSanitizer RACE
 WARNING: ThreadSanitizer: data race
 Write of size 1 by thread T1 · Previous read of size 1 by thread T2
 Location is global 'done1'
```

**Right answer, broken synchronisation.** `volatile` means "do not optimise this access."
It says nothing about atomicity or about making one thread's writes visible to another.
That is the confusion the platform's middle verdict exists to catch: on this machine, today,
it works. It is one compiler upgrade or one different CPU away from not working.

`volatile` is for memory-mapped hardware registers. It has never been a threading tool in
C++.

## 4. Real-world usage

**Why semaphores were invented.** Dijkstra introduced them around 1962–65 for the THE
multiprogramming system, and they are the original synchronisation primitive, older than
mutexes as a distinct concept. The problem he was solving: processes needed to coordinate
without continuously checking on each other, because a process that polls burns a CPU that
another process could be using. `acquire`/`release` were originally `P`/`V`, from Dutch
*proberen* (to test) and *verhogen* (to increment).

**Where you meet them in production:**

- **Connection pools.** `counting_semaphore<100>` for 100 DB connections. A thread acquires
 before borrowing and releases after returning. When all 100 are out, the 101st thread
 sleeps instead of opening a connection the database cannot serve.
- **Rate limiting / bounded concurrency.** "At most 8 uploads at once" is a
 `counting_semaphore<8>` around the upload call.
- **Producer/consumer buffers.** Two semaphores, one counting free slots, one counting
 filled slots, is the classic bounded-buffer solution and needs no condition variable.
- **Thread start/stop handshakes.** Exactly this question's shape: signalling that a phase
 has completed.

**Where NOT to use one:**

- **For mutual exclusion, use a `std::mutex`.** A binary semaphore can fake it, but you lose
 ownership, nothing detects the bug where thread A locks and thread B unlocks. You also
 lose `lock_guard`/`scoped_lock`, so an exception mid-section leaves it acquired forever.
 Mutexes additionally cooperate with `-Wthread-safety` and with TSan's lock-order checking.
- **For "whose turn is it" among many threads.** N threads means N−1 semaphores. One mutex
 plus an integer scales; a forest of semaphores does not. That is follow-up 2.
- **When you need a timeout or cancellation.** `acquire()` waits forever. You want
 `try_acquire_for()`, or a condition variable, or a `stop_token`.

## 5. Performance

Measured on this machine, 200,000 ping-pong round trips between two threads:

| Mechanism | ns per round trip |
|---|---|
| `binary_semaphore` | **144** |
| `mutex` + `condition_variable` | **4782** |
| atomic spin (busy-wait) | 211 |

Two things worth absorbing.

**The semaphore is ~33x faster than mutex + CV here.** A CV handoff requires taking a lock,
signalling, releasing, and the woken thread re-acquiring that same lock, and on a strict
alternation it collides on the mutex every single time. The semaphore has no lock to
re-acquire.

**The semaphore also beat the busy-wait spin**, which surprises people. libc++'s
implementation spins briefly before sleeping, so it captures the fast-path win *and* yields
the CPU when the wait is long. The hand-written spin never yields: it burns a whole core,
and both threads hammer the same cache line so every write invalidates the other's copy.
Spinning is not automatically faster, and it is never free.

## 6. Where this solution fails

The two-semaphore answer is right for this problem. Its limits:

- **A throwing callback deadlocks it permanently.** If `printFirst()` throws, `release()`
 never runs, and `second()` and `third()` sleep forever, the process cannot exit. That is
 follow-up 1, and it is the most common real-world failure of this pattern.
- **Calling `first()` twice is undefined behaviour.** Releasing a `binary_semaphore` already
 holding its one token violates a precondition. Not "throws an exception", plain UB.
- **It does not scale.** N functions need N−1 semaphores, and the class must be edited to
 add one. Follow-up 2.
- **No timeout, no cancellation.** `acquire()` cannot be interrupted. If the thread that was
 supposed to release has died, everyone downstream waits forever with no diagnostic.
- **Single-use.** The gates are consumed. Running the sequence twice on one `Foo` needs a
 reset, and there is no safe moment to reset a semaphore other threads may be waiting on.

## 7. Interview follow-ups

**"first() is called twice by mistake, walk through exactly what happens."** A
`binary_semaphore` holds at most one token. The first `release()` takes it from 0 to 1
(legal); the second `release()`, with nobody having `acquire()`d in between, pushes it
past its own maximum, which the standard leaves as undefined behaviour, not a thrown
exception. In practice this can silently corrupt the semaphore's internal state, so a later
`acquire()` may pass straight through without ever having been signalled for. The guide's
fix (a `counting_semaphore<2>` if double-calls are expected, or an `std::atomic<bool>
has_run` guard if they're a bug to prevent) both convert "silent UB" into either "handled" or
"loudly rejected", pick based on whether a repeated call is a legitimate use case or a
caller error.

**"printFirst() throws, does second() ever run?"** No, and this is the single most common
real failure of this pattern: `sem2.release()` sits *after* the call in program order, so an
exception unwinding out of `printFirst()` skips it entirely. `second()` and `third()` sleep
forever, and since `acquire()` cannot be interrupted, the process can't even exit cleanly.
The fix is exactly what any resource-release-on-exception problem needs, RAII, or an
explicit `catch (...) { sem2.release(); throw; }`, release the gate on the way out
regardless of how you're leaving.

**"Small machine, 1-2 cores.** Does the semaphore-chain still make sense, or does something
change?" The correctness story is identical (a semaphore's wakeup doesn't depend on genuine
parallelism), but the *performance* motivation weakens: with only one or two cores, thread
switches are already expensive relative to the work being coordinated, so the ~33x gap this
reading measured between a semaphore and a mutex+CV round trip narrows in relative terms even
though the absolute per-switch cost is similar, you're paying for a context switch either
way, and that cost dominates more when there's less parallel work happening around it.

**"100 functions need to print in order, the guide suggests a semaphore chain instead of one
mutex+CV+turn-counter, for performance. Do you believe that, and how would you check?"** The
reasoning is plausible, a semaphore handoff doesn't contend on a shared mutex the way
`notify_all` does when 99 threads wake to have 98 of them immediately recheck and resleep
(the exact `notify_all`-vs-`notify_one` cost this course's own FizzBuzz question measures),
but "plausible" isn't "measured." Before trusting it, benchmark both shapes at N=100 on the
actual target hardware, the same way this reading's own 144ns-vs-4782ns numbers were
obtained by running the code, not by reasoning about it.

**"A caller wants to run this sequence twice on the same Foo, how would you support that
safely?"** The semaphores can't simply be reset while other threads might still be observing
them mid-handoff, there's no safe moment to do it from outside. The clean fix is usually to
not reuse the object at all: construct a fresh `Foo` per sequence, or wrap the whole
three-call sequence behind a mutex that serializes entire *runs* of the sequence (not
individual calls), so a second run can only begin once the first has fully drained.

**"How does locking work with semaphores, in plain terms, is a semaphore a lock?"**
(reported Pure Storage question) Not quite. A `std::mutex` has exactly one owner and only
the thread that locked it may unlock it, that ownership is what "locking" usually means. A
`std::binary_semaphore` has no concept of ownership at all: it's just a counter capped at 1,
and `acquire()` blocks while the count is 0, `release()` bumps it to 1 and wakes a waiter.
Nothing stops thread A from `acquire()`ing and thread B from `release()`ing the very same
semaphore, that's not a bug, it's the entire mechanism this question relies on (`first()`
releases a gate it never acquires; `second()` acquires a gate it never releases). A mutex
enforces "only the owner may unlock"; a semaphore enforces nothing about who touches it,
that's the actual distinction to give in an interview, not "a semaphore is basically a
mutex with a counter."
