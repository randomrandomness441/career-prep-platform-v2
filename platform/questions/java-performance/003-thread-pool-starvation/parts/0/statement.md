## ELI5: not enough cashiers vs. cashiers stuck on hold

A store has 8 cashiers. If 200 customers show up at once, most wait in line — that's
just not enough cashiers for the demand. Normal, predictable, fixable by adding more
cashiers or a faster checkout process.

Now picture something different: still 8 cashiers, but each one, partway through
ringing someone up, has to call the warehouse and wait on hold for ten minutes to
confirm a price. All 8 cashiers are now on hold simultaneously. A 9th customer with a
30-second, no-hold purchase is stuck behind all of them, even though their own
transaction would have taken no time at all. Adding a 9th cashier doesn't fix this —
the problem was never headcount.

## What you're actually building (understanding)

A Java service uses a fixed thread pool of 20 threads (`Executors.newFixedThreadPool(20)`)
to handle incoming requests. Under a load spike, p99 latency goes from 40ms to 12
seconds. CPU usage on the box is under 10%. A thread dump taken during the spike shows
this, repeated across most of the pool:

```
"pool-1-thread-7" #23 prio=5
   java.lang.Thread.State: WAITING (parking)
   at java.util.concurrent.locks.LockSupport.park
   at java.util.concurrent.CompletableFuture.join
   at com.example.PricingClient.getPrice(PricingClient.java:41)
   at com.example.OrderHandler.process(OrderHandler.java:18)
```

Nearly every one of the 20 threads is `WAITING`, blocked on a call to a slow downstream
pricing service, not doing CPU work.

## Requirements

1. Is this thread pool exhaustion or thread pool starvation, using the definitions from
   the ELI5 above? Justify it from the thread dump, not just the symptom.
2. Given CPU usage is under 10%, would a CPU flame graph taken during the spike show
   you anything useful here? What would you reach for instead, and why (tie this back
   to question 005 in the base flame graphs pack if you've done it)?
3. "Just add more threads to the pool" is the obvious first reaction. Name a concrete
   reason this makes the underlying problem worse, not better, if the pricing service
   stays slow.

## Why this matters

Both problems present as "requests are slow" and "we ran out of threads." They have
opposite fixes. Treating starvation like exhaustion (throwing more threads at it) is
one of the most common real production mistakes, because the symptom looks identical
from the outside.
