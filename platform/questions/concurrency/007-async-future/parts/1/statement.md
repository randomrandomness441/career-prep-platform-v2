# Build Your Own std::async (with packaged_task)

## ELI5: the drive-thru order ticket

You pull up to a drive-thru, place an order, and get handed a ticket number. You don't
stand there watching the kitchen cook, you drive around, park, and whenever you're
ready you walk up and trade your ticket for your food. That's `std::future`: a ticket
you can redeem later, for a result that's being produced somewhere else, right now,
without you waiting on it.

And here's the part that makes it actually useful: if the kitchen burns your order, they
don't just... not tell you and hope you never notice. When you redeem your ticket, they
tell *you*, specifically, what went wrong, not some random customer, not nobody. `std::async`
gives you this whole setup for free. This question asks you to build the mechanism
yourself, on top of `std::packaged_task`, the piece that actually wires "run this
function" to "hand out a ticket" to "whoever redeems the ticket gets the result *or* the
error."

## What you're actually building

```cpp
template <typename F, typename... Args>
std::future</* return type of F(Args...) */> spawn_task(F&& f, Args&&... args);
```

## Requirements

1. **The task runs on a thread of its own**, never on the calling thread, and
 `spawn_task` itself must not block waiting for it. You hand out the ticket
 immediately; the kitchen starts cooking in the background.
2. **The caller gets a real value back.** `f.get()` on the returned future returns the
 task's result, works for `void`-returning callables too, and forwards arguments
 correctly, including move-only arguments and move-only callables.
3. **An exception thrown inside the task comes back out of `get()` as itself.** Not
 `std::future_error` (`broken_promise`), and definitely not `std::terminate`, an
 uncaught exception on a raw `std::thread` terminates the whole process; this must
 never happen here.
4. **The caller never joins anything.** The future, the ticket, is the only handle it
 gets. Nothing about the task's lifetime requires the caller to hold on to a
 `std::thread` object.
5. Many tasks may be in flight from different calls to `spawn_task` at once, many
 customers, many kitchens, many tickets, no cross-talk between orders.

## Why the constraints exist

**Everything the task touches, the callable and its arguments, must be safe to use
after `spawn_task` returns,** even though the caller's own arguments may be temporaries
that don't outlive the call. The kitchen can't cook with ingredients that vanish the
moment you drive off; whatever it needs has to be copied or moved into its own kitchen
before you leave the window.
