# A Thread-Safe Event Gate

## ELI5: a ride that's closed for maintenance

An amusement park ride sometimes closes briefly for maintenance. While it's closed, people
who walk up don't just leave, they add their name to a waiting list. The instant
maintenance finishes, the ride reopens and everyone on that waiting list gets to go, all
at once, in the order they signed up. After that, if you walk up and the ride's just
open, no maintenance happening, you walk straight on. No waiting list, no delay.

That's the whole shape of this question. Some "event" (the maintenance) happens on its
own schedule, started and finished by someone else. Meanwhile, users keep showing up
wanting to register a callback. If the event is in progress, their callback goes on the
waiting list instead of running. The moment the event ends, the whole waiting list runs,
in order. Anyone who shows up after that runs immediately, same as always.

```
Event in progress
----|---------------|--------------------|-------------------|--------------> timeline
U1: reg_cb(f1) U2: reg_cb(f2) Event completed U3: reg_cb(f3)
 (runs f1, then f2) (runs f3 immediately)
```

## What you're actually building

```cpp
class EventGate {
public:
    EventGate();

    // Called by whichever thread manages the event.
    void begin_event(); // marks an event as in progress
    void end_event(); // marks it complete; runs every callback that arrived
    // while it was in progress, in registration order

    // Called by any number of user threads, at any time.
    void reg_cb(std::function<void()> cb);
};
```

`reg_cb`'s contract: if no event is in progress right now, it runs `cb` immediately,
before returning, no waiting list, no delay. If an event *is* in progress, it queues
`cb` instead, and returns without running it; that callback runs later, when
`end_event()` drains the waiting list.

## Requirements

1. `reg_cb` called while no event is in progress runs `cb` synchronously, before
 `reg_cb` returns.
2. `reg_cb` called while an event is in progress does **not** run `cb`, it queues it.
3. `end_event()` runs every queued callback, **in the order they were registered**, and
 the waiting list is empty once it returns.
4. A `reg_cb` call that arrives after `end_event()` has already finished runs
 immediately, same as requirement 1, even if it's racing right at the boundary. The
 callback that shows up "just after the ride reopens" walks straight on; it never gets
 stuck waiting for some *future* maintenance window it wasn't even around for.
5. Many threads call `reg_cb` concurrently, during and outside events, and every single
 callback ever registered runs **exactly once**, never lost, never duplicated, no
 matter how the timing lines up.

## Why the constraints exist

**The boundary is the entire exercise.** A `reg_cb` call happening at the exact instant
`end_event()` is draining the waiting list has to resolve cleanly one way or the other:
either it gets into the batch `end_event()` is about to run, or it sees no event in
progress and runs immediately. There is no third outcome where it gets queued into a
waiting list that's already been drained and forgotten, that callback would simply
never run, permanently, and nothing about the program would ever tell you it happened.
That silent-loss failure mode is why this question is worth doing carefully instead of
reaching for the first thing that compiles.
