## 1. Reframe the problem

"Detect a deadlock" sounds like it should mean *notice two threads are stuck waiting on each
other and do something about it*. Standard C++ gives you no way to do the second half,
there is no API to inspect another thread's lock-wait state, and no safe way to forcibly
stop a thread that's blocked inside a `mutex::lock()` call. Whatever it's holding, it keeps
holding.

So reframe the problem as the thing you actually *can* build: don't wait for an operation
you expect to finish, wait for it *or a deadline*, and if the deadline wins, report that
and move on with your life. This isn't detecting the deadlock's cause; it's refusing to
become a second victim of it. The distinction matters because it changes what "correct"
means here: a watchdog's job is to never hang, not to fix the thing that's stuck.

## 3. The broken version, first

The boilerplate is one line, `run_with_deadline` simply calls `op()` and returns `true`.
`deadline` is a parameter that's accepted and never looked at again:

```cpp
bool run_with_deadline(std::function<void()> op, std::chrono::milliseconds deadline) {
    op();
    return true;
}
```

**Why it looks right:** it satisfies the type signature, it compiles clean, and for any
operation that actually finishes reasonably quickly, which is every operation you'll
manually try while writing and testing this, it returns the right answer, `true`. Nothing
about running `op()` synchronously *looks* wrong until you specifically test the case this
class exists for: an operation that's slow, or stuck.

Running it, a 300ms operation given a 50ms deadline:

```
a 300ms operation given a 50ms deadline was reported as completed -- the deadline was never checked
```

Not just wrong, the *specific* kind of wrong that matters: it reported `true` (success)
for an operation that blew through its deadline by 6x, because there was never any code
path that could return `false`. A watchdog that can't report a stall is worse than no
watchdog at all, it actively tells you everything is fine.

The fix, run `op` on its own thread, signal completion through a promise, and race that
signal against the deadline with `future::wait_for`:

```cpp
std::future_status status = signal.wait_for(deadline);
if (status == std::future_status::ready) { worker.join(); return true; }
worker.detach();
return false;
```

Same 300ms-op-vs-50ms-deadline case, plus the full suite (a fast success case, five trials
of a genuine two-mutex deadlock, and thirty trials of safe same-order contention):

```
timeout without a deadlock, fast success, 5 real-deadlock trials all detected, and
30 contended-but-safe trials with zero false positives
```

The 300ms case now returns `false` at roughly the 50ms mark, not after waiting out the full
300ms, the naive version's other failure (even if it eventually *did* check the deadline
after the fact, waiting for a stuck operation to finish before reporting that it's stuck
defeats the purpose just as completely).

## 6. Where this solution fails

- **A thread stuck in a real deadlock leaks for the life of the process.** `detach()` is the
 only safe option once the deadline wins, there's no way to know if `op` will ever return,
 and joining would just make `run_with_deadline` hang too. Whatever mutexes that thread is
 holding stay locked forever. This isn't a bug to fix; it's the actual cost of C++ having no
 safe thread-cancellation primitive. A long-running service that hits this repeatedly
 accumulates leaked, permanently-blocked threads until something else gives out (thread
 handle exhaustion, memory).
- **The deadline is approximate, not exact.** `wait_for` guarantees it returns *no earlier*
 than the deadline if the promise isn't fulfilled, but scheduling delays (a busy machine, a
 preempted watchdog thread) can push the actual report time later. Don't build logic that
 depends on the deadline firing at a precise millisecond.
- **This detects a stall, not its cause.** `run_with_deadline` returning `false` tells you
 *that* something didn't finish in time, a real deadlock, a slow disk, a network call that
 will never come back, and "op is just doing 10x more work than expected" all look
 identical from here. Diagnosing *which* requires the tools in section 7, not this class.
- **Every call spends a full thread on `op`**, even for operations that almost always finish
 well inside the deadline. For a hot path called constantly, that's real overhead against
 operations that virtually never stall, a watchdog like this is suited to occasional,
 high-value checks (a startup step, a critical external call), not a wrapper around every
 function call in a program.
- **Two independent watchdogs can each correctly detect the same deadlock and each leak
 their own thread**, as the test's five deadlock trials show, both `wd1` and `wd2` report
 `false`, and both of their worker threads are now stuck holding a lock forever. The
 watchdog doesn't coordinate with anyone else who might be waiting on the same resource; it
 only ever reports on the one call it was given.

## 7. Interview follow-ups

**"How would you actually find a deadlock in production without reading through the code
line by line?"** Attach a debugger to the live process (`gdb -p <pid>`, then
`thread apply all bt` to dump every thread's stack at once) and look for multiple threads
stuck inside a mutex-lock call, each backtrace showing a different lock being waited on,
that's the live version of exactly what this question's test constructs on purpose. TSan
(this course's own `./check` and the platform's runner both use it) can also catch some
lock-order violations *before* they deadlock, by tracking the order locks are acquired in
across runs, though it only flags what it happens to observe, not every possible ordering.

**"What's the Banker's Algorithm, and would you actually use it here?"** A resource
allocation strategy that checks, before granting a lock, whether granting it could ever lead
to a state where no thread can finish (an *unsafe* state), if so, it makes that thread wait
even though the resource is currently free. It's the classic deadlock-*avoidance* answer in
OS textbooks, but it needs to know every thread's full future resource needs in advance,
which real code essentially never can declare. Watchdogs and lock ordering (see
[[021-dining-philosophers]] and [[006-hierarchical-mutex]]) solve the same problem with
information you actually have at compile time or acquisition time, which is why they're what
real C++ codebases reach for instead.

**"This watchdog can't tell a real deadlock apart from a legitimately slow operation, does
that matter in production?"** Yes, and it's the reason a watchdog alone isn't a complete
answer: a false alarm on an operation that's merely slow under load causes you to abandon
(leak) a thread that would have finished fine, while a deadline set too generously means a
real deadlock goes unreported for a long time. In production this is usually paired with
metrics (how long has *this specific* operation historically taken?) to set the deadline
adaptively, and with the debugger-based technique above to distinguish "slow" from "stuck"
once a stall is reported, rather than trying to make the watchdog itself smart enough to
tell the difference.

**"You have thousands of these watchdog-guarded calls running per second, what would you
monitor?"** The stall rate itself (how often `run_with_deadline` returns `false`) as a
live metric, alerting on any nonzero sustained rate rather than treating individual stalls
as isolated incidents, a watchdog that starts firing repeatedly on the same call site is
telling you about a real, ongoing problem (a newly-introduced lock-order bug, a downstream
service that started hanging), and the aggregate rate is a far more actionable signal than
any single leaked thread.
