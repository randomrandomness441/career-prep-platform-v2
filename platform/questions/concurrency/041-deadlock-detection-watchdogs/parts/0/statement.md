# Deadlock Detection & Watchdogs

## ELI5: a kitchen timer for someone else's task

You hand a coworker a task and set a kitchen timer for it. If they finish before the
timer dings, great, you find out and move on immediately, you don't stand around
waiting for the timer to actually ring first. If the timer dings and they're *still* not
done, you don't keep standing there, you report "not done in time" and get on with your
day. You can't reach into their office and force them to stop; if they're truly stuck
(say, in a deadlock with someone else), they might genuinely never finish, and whatever
they were holding onto stays held forever. All you can honestly promise is: *you'll never
personally wait past the timer.*

That's exactly what this class does. You can't fix a deadlock from the outside, standard
C++ has no way to forcibly stop another thread. What you *can* do is refuse to wait for
it forever, and say so.

## What you're actually building

```cpp
class Watchdog {
public:
    // Runs op somewhere else, with a deadline. Returns true if op finished
    // in time, false if the deadline elapsed first.
    bool run_with_deadline(std::function<void()> op, std::chrono::milliseconds deadline);
};
```

## Requirements

1. If `op` finishes before `deadline` elapses, return `true` promptly, don't wait out
 the full deadline once the work is already done, the same way you'd notice your
 coworker finished early rather than staring at the timer anyway.
2. If `deadline` elapses before `op` finishes, return `false` **at (approximately) the
 deadline**, not whenever `op` eventually finishes (if it ever does). `run_with_deadline`
 must not hang just because `op` hangs.
3. **`op` may never return at all, a genuine deadlock.** `run_with_deadline` must still
 return `false` at the deadline in that case, without crashing, without undefined
 behaviour, and without blocking the caller forever.
4. **No false positives.** Legitimate work that's merely slow, or contended-but-safe
 (multiple callers taking the same two locks in the same order), must be reported as
 completed if it finishes within its own deadline. A slow coworker isn't a stuck one.

## Why the constraints exist

**You cannot forcibly terminate a `std::thread`.** If `op` is genuinely stuck, accept
that the thread running it, and anything it's holding, outlives this call; detect and
report, don't attempt to fix. There's no reaching into someone's office and physically
stopping them mid-task.
