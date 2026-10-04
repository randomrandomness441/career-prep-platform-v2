## 1. Reframe

"We ran out of threads" is a symptom description, not a diagnosis. Two different root
causes produce it: not enough threads for the demand (exhaustion), or enough threads
that are all stuck (starvation). The fix for one makes the other worse.

## 3. The broken version, first

The natural first reaction to "requests are slow, threads all busy" is to add more
threads — that's the right fix for exhaustion and actively counterproductive for
starvation, since it doesn't touch the actual bottleneck (the slow downstream call) and
can increase load on it. Without a thread dump or similar evidence, the two look
identical from a dashboard showing "pool utilization: 100%."

## 4. Interview follow-ups

- What's the specific thread-dump signal that separates starvation from a thread that's
  simply doing real, expensive CPU work? `RUNNABLE` (actually executing) vs. `WAITING`/
  `TIMED_WAITING` (parked, not executing) — a pool full of `RUNNABLE` threads under real
  CPU load is a different problem (genuinely needs more capacity or faster code) than a
  pool full of `WAITING` threads (needs the dependency fixed, not more threads).
- How would this same failure look different with Java virtual threads instead of a
  fixed platform-thread pool? Virtual threads are cheap enough that "not enough of them"
  stops being the bottleneck — but a blocking call that pins a virtual thread to its
  carrier (see question 010) can reproduce the exact same starvation shape at the
  carrier-pool level instead of the application-thread-pool level.
