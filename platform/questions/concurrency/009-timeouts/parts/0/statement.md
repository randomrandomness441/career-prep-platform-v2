# Waiting With a Deadline

## ELI5: the pizza delivery analogy

You order a pizza. You're hungry, so you tell the cashier: "I'll wait exactly 10 minutes.
If it's not here by then, I'm leaving."

- **The pizza arrives**, you grab it and leave. Done.
- **The chef yells from the back, "still working on it!"**, that's not the pizza, so you
 look, see nothing, and go back to waiting. (This is a *spurious wakeup*: someone rang
 the bell, but the thing you're actually waiting for still isn't here.)
- **The catch, and the whole point of this question:** every time the chef yells, a
 careless implementation restarts your 10-minute timer from scratch. Chef yells every
 9 minutes, forever, and you never leave, you're still "waiting 10 more minutes" 100
 minutes later. Your job is to write the waiting logic so that no matter how many times
 the chef yells, you still walk out the door at the 10-minute mark, not a second later.

## What you're actually building

A one-shot mailbox that holds a single integer, which any number of waiters can wait on
with a deadline:

```cpp
class result_slot {
public:
    // Drops the value into the mailbox. Wakes up everyone waiting for it.
    void set(int value);

    // Rings the bell for no reason -- simulates the chef yelling. Nothing about
    // the mailbox's contents changes; it just wakes sleeping waiters up so they
    // can check ("is it here yet?") and, finding it isn't, go back to sleep.
    void nudge();

    // Wait up to `timeout` for a value. Returns it if it shows up in time,
    // std::nullopt if your patience runs out first.
    std::optional<int> wait_for_result(std::chrono::milliseconds timeout);
};
```

**The hard requirement:** you must work out your exact "leave by" time the moment
`wait_for_result` is called. If someone asks for a 1000ms timeout and `nudge()` fires at
900ms, you wake up, check the mailbox, find it empty, and go back to sleep, but you only
have 100ms of patience left, not another full 1000ms. You must walk out at the 1000ms
mark no matter how many times you were nudged in between.

## Requirements

1. If a value is already there when you call `wait_for_result`, return it immediately,
 don't even go to sleep first.
2. If a value arrives while you're waiting, return it promptly.
3. If nothing ever arrives, return `std::nullopt` once your timeout is up.
4. **`wait_for_result` must never block appreciably longer than `timeout`, no matter how
 many times `nudge()` is called while it's waiting.** This is the whole exercise, see
 the pizza story above.
5. Any number of threads may be waiting at once; all of them must see the value once it's
 set, everyone's pizza order gets fulfilled from the same delivery.
6. A `timeout` of `0ms` is legal: it just means "check right now, don't wait at all",
 return the value if it's already there, otherwise `nullopt`, immediately.

## Why the constraints exist

- **No `sleep()`-and-poll loops**, you can't write `while (!ready) sleep(10);`. That
 burns CPU the whole time you're "waiting," like pacing in circles instead of sitting
 down. Use `std::mutex` + `std::condition_variable`, which actually puts the thread to
 sleep until something wakes it.
- **Use `std::chrono::steady_clock`, not `system_clock`.** `system_clock` is the everyday
 wall clock, and something on the machine (NTP syncing the time over the internet, or a
 user changing the clock) can step it *backwards*. If your 10-minute deadline is a point
 on a clock that just jumped back 5 minutes, you now wait 15 minutes instead of 10.
 `steady_clock` is a stopwatch: it only ever ticks forward and nothing can reset it.

In short: this question is really just "implement a condition-variable wait with a
timeout", but it's testing whether you reach for `wait_until` (an absolute deadline,
computed once) instead of `wait_for` (a relative duration, which restarts every time you
re-enter the wait). The tests measure real elapsed wall time and will fail you for
overrunning the requested timeout, so this isn't something you can talk your way past.
