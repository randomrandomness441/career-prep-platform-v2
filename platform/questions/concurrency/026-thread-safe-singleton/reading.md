## 1. Reframe the problem

Lazy initialization is a **check-then-act**: *read* "does it exist?", then *write* "create
it". Two operations, with any number of threads free to run between them. That gap is the
entire problem, and it has two different failure modes people usually collapse into one.

The obvious one: two threads both read "no", both create. Two constructions, one leaked,
threads holding pointers to two different objects that each believe is *the* singleton.

The quiet one is worse. Thread A publishes the pointer. Thread B reads it and calls a
method, and is allowed to see the pointer but **not the fields the constructor wrote**.
Why: on a multicore machine, core 1 finishing its writes and core 2 noticing them are two
different events, and without a synchronisation edge between the threads, nothing forces
core 2's cache to have caught up. The name for that edge is *happens-before*, a guarantee
that one thread's writes are visible to another thread's next read. Publishing a plain
pointer creates no such edge. So the bug is not only "it might build twice"; it is "even
if it builds once, the object may look half-built to whoever finds it".

So the reframe: **the question is not "how do I make construction atomic", it is "what
creates a happens-before edge between the constructor and every caller".** C++11 answered
this in the language itself for one construct, a `static` local variable, and the whole
exercise is knowing that.

## 3. The broken version, first

```cpp
template <typename T>
class singleton {
    inline static T* inst_ = nullptr;
public:
    static T& get() {
        if (inst_ == nullptr) // read
        inst_ = new T(); // write, and between the two, anything can run
        return *inst_;
    }
};
```

**Why it seems correct:** because it behaves correctly, nearly always. Run the harness
tests against this version and all 12 runs pass; run the binary standalone 50 times and
it passes 50/50. Thread creation itself takes tens of microseconds, so callers arrive
staggered, the first one builds, everyone else finds a non-null pointer. The constructor
counter reads exactly 1. Every check you could run on a quiet machine says *fine*. That
is what makes it seductive: it is not a bug that shows up; it is a bug that waits for
load, for a scheduler hiccup, for the day the machine is busy.

Here is what the harness's ThreadSanitizer stage says about the same code that just
passed 12/12 (tidied to your frames; run on this machine):

```
WARNING: ThreadSanitizer: data race (pid=2675)
 Read of size 8 at 0x000104ab4010 by thread T2:
 #0 ...hammer_once<0>()::'lambda'()... (tests_tsan:arm64+0x100001b44)
 Previous write of size 8 at 0x000104ab4010 by thread T1:
 #0 ...hammer_once<0>()::'lambda'()... (tests_tsan:arm64+0x100001b80)
 Location is global 'singleton<config<0>>::inst_' at 0x000104ab4010
 ...
SUMMARY: ThreadSanitizer: data race
```

Read of the pointer by one thread, previous write by another, nothing between them. The
"Location" line names the exact culprit: `singleton<config<0>>::inst_`.

And when the stagger goes away, ten threads released from a start gate at the same
instant, with a constructor that takes 100 µs of "work", the version above did this,
three runs out of three:

```
constructions: 10, distinct addresses handed out: 10
constructions: 10, distinct addresses handed out: 10
constructions: 10, distinct addresses handed out: 10
```

Ten threads, ten singletons, ten leaks. Every thread "has the singleton" and no two of
them mean the same object. This is the same code that passed 50/50 quiet runs.

**The fix** is one line, and it is old enough to carry a name:

```cpp
static T& get() {
    static T instance; // "Meyers singleton"
    return instance;
}
```

Since C++11 the standard guarantees, for a `static` local: exactly one thread runs the
initializer, concurrent callers **wait** for it, and when they proceed, everything the
initializer wrote is visible to them, the compiler emits a hidden guard variable that
creates the happens-before edge for you. If the constructor throws, the initialization is
marked not-done and the next call tries again (verified on this machine: first call
throws, second call constructs, attempt counter reads 2).

What that compiles to after initialization is worth seeing, real disassembly of `get()`
from this machine, arm64:

```
__Z3getv:
 adrp x8, __MergedGlobals@PAGE+8
 add x8, x8, __MergedGlobals@PAGEOFF+8
 ldaprb w8, [x8] ; acquire-load the guard byte
 tbz w8, #0, LBB0_2 ; bit clear -> slow path (first time only)
 adrp x0, __MergedGlobals@PAGE
 add x0, x0, __MergedGlobals@PAGEOFF
 ret ; return the address, done
```

One acquire load and a branch. Measured on this machine: **0.45–0.48 ns per call** after
initialization (200M calls). The guarantee is not free, you pay one ordered load
forever, but it is about as cheap as a guarantee can be.

The manual alternative, for when the "once" is not a construction or the object cannot
live in the function: `std::once_flag` + `std::call_once`, which makes the same
promise explicitly. Senior-level answer: the static local first, `call_once` when the
shape doesn't fit. Pre-C++11 people wrote "double-checked locking", check, lock, check
again, and it was the canonical broken pattern of that era, because the *publishing*
store needed ordering the language couldn't then express. Magic statics made the whole
dance unnecessary.

## 6. Where this solution fails

- **Initialization is thread-safe; the object is not.** The guarantee covers the moment
 `get()` returns. If two threads then call a mutating method on the singleton, that is
 an ordinary shared-state race and the singleton gave you nothing. This is the most
 common wrong belief about this pattern: "it's a singleton, so it's thread-safe."
 No, it's *constructed* safely. Using it is your problem.

- **Destruction is nobody's guarantee.** The static is destroyed during static
 destruction, in some order relative to every other static in the program. A background
 or detached thread that calls `get()` after that moment gets a reference to a
 destroyed object, silent UB. If the singleton must outlive everything, the standard
 production dodge is to deliberately leak it: `static T& get() { static T* p = new T; return *p; }`
 , never destroyed, never used after destruction. Ugly, and exactly right for
 loggers and similar process-lifetime machinery.

- **Recursive initialization is undefined behaviour.** If `T`'s constructor somehow calls
 `get()` again, directly or three levels down, the standard says UB. On this machine
 libc++ detects it and aborts loudly:
 `__cxa_guard_acquire detected recursive initialization: do you have a function-local static variable whose initialization depends on that function?`
 (exit 134). The naive pointer version doesn't get a message: its recursion is an
 endless `new` chain that segfaults (exit 139 on this machine).

- **Lazy means someone pays on first use.** Under lazy init, whichever request arrives
 first constructs the object *and every concurrent caller blocks on the guard until it
 finishes*. If construction is slow (config parse, connection pool warm-up), your first
 request under load carries all of it plus a queue behind it. When you can construct at
 startup, do; laziness is for when startup genuinely cannot supply the dependencies.

- **The pattern itself is the failure mode at scale.** A singleton is hidden global
 state: any function in the program may depend on it without saying so, tests can't
 substitute a fake without a seam, and shutdown order between singletons becomes an
 archaeological dig. Production codebases spend real money removing these. Use one when
 there is exactly one of a thing *by the nature of the process* (a logger, a metrics
 registry), not to avoid passing a parameter.

- **It cannot be reset, so it cannot be selectively re-tested.** The static's guard is
 forever; tests that need a fresh instance need a fresh process or a template
 parameter, which is why this exercise uses `singleton<T>` rather than one global class.

## 7. Interview follow-ups

**Q: How is the static local actually made safe, what does the compiler do?**
A: It emits a hidden guard variable beside the object. First arrival runs the
constructor; anyone arriving mid-initialization waits; the guard's release pairs with
the waiters' acquire, which is the happens-before edge that makes the constructor's
writes visible. After initialization the fast path is one acquire load and a branch,
0.45 ns measured here. The slow path (`__cxa_guard_acquire` on this ABI) involves a
mutex and only runs once.

**Q: The constructor throws. Is the singleton now permanently broken?**
A: No. Initialization is not marked done; the next call attempts construction again.
Verified here: first `get()` throws, second constructs, attempt counter = 2. Caveat
worth volunteering: if the constructor *always* throws, every caller pays the throw,
a retry storm is a real outage shape. If failure is persistent, cache it deliberately.

**Q: So after `get()` returns, my threads can use the singleton freely?**
A: No. Initialization thread-safety ends the instant `get()` returns. Member access
needs its own mutex or atomics. This is the follow-up that separates people who have
read the pattern from people who have debugged it.

**Q: Two cores, one busy with an interrupt storm, does the naive version get safer?**
A: It gets *worse*. The stagger that hides the bug comes from threads proceeding without
preemption; fewer cores means more preemption, which means more chances for the
scheduler to park a thread exactly between the read and the write. The race's
probability goes up as the machine gets busier or smaller, the 10/10 double
construction above is the same code, just released together.

**Q: 64 cores, 64 threads hit first `get()` at the same instant, what happens, and what
does it cost?**
A: With the naive version: up to 64 constructions and 64 leaks, threads holding 64
mutually inconsistent objects, plus reads of half-built fields. With the fixed version:
exactly one construction and 63 waiters parked on the guard, correct, but the *first*
request after deploy absorbs construction while 63 requests queue behind it. That is why
production services warm singletons at startup, before accepting traffic.

**Q: 10^8 requests/second, is the guard load a bottleneck?**
A: No. After initialization it is a read-only cache line, one acquire load per call,
shared cleanly by every core; it scales. The scaling risk is *early* contention (the
construction window above) or the singleton itself being a serialisation point, one
mutex inside the object that every request touches. The guard is never the problem; what
the object does when you use it usually is.

**Q: How would you prove in production that construction ran exactly once?**
A: A counter or log line in the constructor, which, note, is itself most cleanly done
with the same trick: `static const bool once = (log("built"), true);`. If that log fires
twice, you have found either a non-magic static or two different singleton objects in
one process (shared libraries loaded twice, check your `dlopen` paths).

**Q: Why a static local rather than a global at namespace scope?**
A: Namespace-scope objects with dynamic initialization are initialised in an
unspecified order across translation units, your global's constructor may run before
the thing it depends on exists (the "static initialization order fiasco"). The function-
local static defers initialization to first use, when all dependencies are callable, and
gets the thread-safety guarantee for free. Same object lifetime, none of the ordering
roulette.
