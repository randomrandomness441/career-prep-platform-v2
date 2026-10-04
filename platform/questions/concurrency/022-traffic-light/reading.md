## 1. Reframe the problem

This is not "toggle a boolean when a car arrives." It is a **group handover**
problem. The bridge is a resource that is exclusive *between* two groups and
shared *within* one group. Reader-writer locks have exactly this shape. Here
the groups are the two axes.

Once you see it that way, the real question is narrow and sharp: **at which
moments may the light change, and which thread is running code at those
moments?** There are exactly two safe moments. A car arriving at a red axis
when the bridge happens to be empty, and the last car leaving an empty bridge.
Every correct solution is built around making both moments reachable. The
published solution this question comes from made neither reachable. It still
looks right at a glance, which is why it is worth studying.

One more thing hides in the problem statement. "Switch when the axis empties"
is not enough, because an endless stream on the green axis never empties
*arrivals*, only the bridge. Starvation-freedom needs a closure rule: the
green axis closes to newcomers once an opponent is queued.

## 3. The broken version, first

This is the source guide's own solution, with its logic intact and names
adapted. Read it and try to find the bug before the output below.

```cpp
void carArrived(bool wants_ns) {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [wants_ns]() {
        return (wants_ns && is_green_ns) || (!wants_ns && !is_green_ns);
    });

    if (wants_ns && !is_green_ns) { is_green_ns = true; }   // turn the light
    if (!wants_ns && is_green_ns) { is_green_ns = false; }  // turn the light

    current_passing++;
    crossCar();
    current_passing--;

    if (current_passing == 0) cv.notify_all();
}
```

**Why it seems correct, and who it fools.** It has every ingredient the
checklists tell you to look for: wait with a predicate, a count of who is on
the bridge, notify when the bridge empties. A reviewer pattern-matching on
those ingredients passes it. Single-axis traffic even *runs* correctly. The
bug is not a missing ingredient. It is the order of two thoughts: "wait until
it is my turn" and "make it my turn." The wait only returns when the light
**already** matches this car, so by the time the two `if` branches run, their
condition is provably false. The light-turning code is dead code. Nobody ever
changes the light.

Real output: three north/south cars, then one east/west car, then three more
north/south. The watchdog prints after 2 seconds and exits.

```
N/S car 0 crossed
N/S car 5 crossed
N/S car 1 crossed
N/S car 2 crossed
N/S car 4 crossed
N/S car 3 crossed
WATCHDOG after 2s: N/S crossed 6, E/W crossed 0
exit code: 3
```

Six cars crossed, the seventh is still parked in `cv.wait`, and will be there
when the machine is recycled. In the harness this is a hang. The test runs
cars through *both* axes and the run never finishes.

The fix has two halves. First, move the light change to the two moments where
it is reachable. On arrival, a car arriving at red closes the green axis, and
if the bridge is empty and its axis has a queue, the light switches right
there. On departure, the last car off an empty bridge hands the light to the
queued axis. Both happen under the lock, with `notify_all` whenever the light
actually moved. Second, track the state this needs: cars on the bridge, cars
queued per axis, and a `closed` flag that stops newcomers on the green axis
once an opponent is queued. That flag is what makes the handover bounded
under an endless stream.

There is a second, quieter bug in the same snippet, and it survives many
"fixed" versions. The crossing runs while the lock is held. The light becomes
a stop sign: cars on the same axis take the bridge one at a time. Measured on
this machine, 8 cars on one axis, 1 ms crossing each, with the light-turning
bug fixed but the lock still held through the crossing:

```
8 N/S cars, 1 ms crossing each: wall time 10 ms, max on the bridge together: 1
```

The correct version, crossing with the lock released:

```
8 N/S cars, 1 ms crossing each: wall time 1 ms, max on the bridge together: 8
```

Ten times faster on the same hardware, because the bridge was never the
bottleneck. The lock held across it was. That is why requirement 3 exists.
`cross_car()` must run with the lock released, guarded by an RAII "ticket"
that still empties the bridge if the callback throws.

## 6. Where this solution fails

- **Every handover is a small thundering herd.** One condition variable means
  `notify_all` wakes parked cars of *both* axes, and the ones still red
  re-check and re-park. With a handful of cars that is noise. With hundreds
  queued it is a real cost, and it is exactly the subject of the next question
  (025). The targeted fix is one condition variable *per axis*, so a handover
  wakes only the newly green side.
- **No fairness inside an axis.** Which queued car of the green axis wakes
  first is unspecified. The standard says nothing about condition-variable
  wakeup order. Platforms differ, some futex wake paths are roughly FIFO in
  practice, but you cannot build a bus-lane priority on that. If you need
  per-axis FIFO, buses before cars, the queue must become explicit data, not
  cv order.
- **A long batch is a long wait.** The handover happens when the bridge
  empties, not on a timer. One slow truck crew can hold the light as long as
  its axis keeps supplying cars. Real controllers bound the green with a max
  phase length or max cars per phase, the same idea as the closure flag but
  time-based.
- **Exceptions from the crossing.** The RAII ticket keeps the *state*
  consistent if `cross_car()` throws, but the exception still propagates out
  of `cross()` into the car's thread. That's fine if cars are `jthread`s with
  a handler. A raw `std::thread` whose thread function escapes an exception
  calls `std::terminate`.
- **One lock for the whole intersection.** All arrivals serialise on one
  mutex. At human-scale arrival rates the critical section is a few hundred
  nanoseconds and this is irrelevant. At high rates the lock, not the bridge,
  saturates first, and the answer becomes per-approach locks or a lock-free
  arrival path.
- **Preemption is not really possible.** A car physically on the bridge
  cannot be removed. "Emergency preemption" can only mean closing the green
  axis to newcomers *now* and forcing the handover at the next empty-bridge
  instant. Anything promising instant green is lying about physics.

## 7. Interview follow-ups

**Q: Why track `current_passing` at all, instead of toggling the light after
each car?**
Because the entire value of a light over a stop sign is the batch. Toggle per
car and you have built a stop sign, one car on the bridge at a time, every
car paying a full handover. Measured above, 8 cars took 10 ms serialized
versus 1 ms batched. `current_passing` is how the code knows the bridge
emptied without holding the lock through the crossing.

**Q: Endless north/south traffic. Does east/west ever starve, and how do you fix it?**
The closure flag *is* the fix. Once an east/west car is queued, the green
axis admits no newcomers, so the stream can only finish what is already on
the bridge and then must yield. Without it, east/west gets the light only if
the north/south stream happens to pause. The cruder alternative, a max car
count per green phase, also works and is what real controllers do, since
their phases are time-bounded. But it trades fairness for a knob. Too small
and throughput collapses to handover overhead, too large and wait time is
unbounded. `cv.wait_for` with a timeout is the wrong tool. A timeout does not
change who is *entitled* to go, it just adds spurious wakeups and still lets
the stream barge back in.

**Q: An emergency vehicle arrives on east/west while north/south is
streaming. Preempt?**
Close the north/south axis to newcomers immediately, setting the closure flag
on its behalf, and if the bridge is non-empty, mark the emergency axis as the
forced next holder so the last car off the bridge hands the light to it.
Add a preemption class to the state so a *second* emergency car on the other
axis queues behind the first rather than fighting it. What you cannot do is
claim instant green. Cars mid-bridge finish crossing no matter what the
light says.

**Q: Scale it to a busy 16-lane intersection doing 10^8 arrival checks a
second. What saturates first?**
Not the bridge. Three things break in order. First, the single mutex: every
arrival, admission, and departure takes it, and its cache line bounces
between cores. The fix is splitting state per axis, then per approach.
Second, the single condition variable: every handover `notify_all`s every
parked thread on both axes, and each wake is a futex syscall at roughly
1 to 2 microseconds, so thousands of parked cars make the wake the dominant
cost. One cv per axis, then per approach, cuts the herd geometrically. Third,
the state word itself: once you shard, keep each shard's mutex, cv, and
counters on their own cache line, 128 bytes on this arm64 machine, or the
sharding buys nothing because the shards share a line and the line, not the
mutex, becomes the contention point.

**Q: Same controller on a single-core embedded board, one core reserved for
the OS. What changes?**
Nothing in the design. There is no spinning or polling anywhere, parked
threads cost nothing on a single core, and handovers are scheduling events,
not busy work. What becomes real is **priority inversion**. If the crossing
callback runs at low priority and an emergency thread at high priority waits
for the handover, an unrelated medium-priority thread can extend the wait
indefinitely on a single core. The short critical sections help, since the
lock is never held across the crossing, but on an RTOS you want a
priority-inheritance mutex, and you must bound how long a low-priority car
may hold the "empty bridge" moment. `std::mutex` gives you none of that
portably. It is an OS knob.

**Q: A car crashes mid-crossing, thread throws, thread dies, or process
dies. What happens to the intersection?**
Three different cases. If the callback throws, the RAII ticket's destructor
still runs during unwinding, relocks, decrements, and performs the handover.
The intersection stays consistent, and that is why the ticket exists instead
of two loose lines after the callback. If the car's thread is killed while
*parked*, that's harmless. It holds nothing, it is just gone. Its queue entry
is a counter you should decrement in a cleanup path, or that arrival never
happened, depending on how you cancel. If the thread dies while *holding the
mutex*, `std::mutex` stays locked forever. There is no robust-mutex story in
the standard. POSIX has `PTHREAD_MUTEX_ROBUST`, but the C++ API does not
expose it, so the whole intersection deadlocks. The production answer is a
watchdog that detects the stall and restarts the controller process,
accepting that cars queued at the time re-arrive.
