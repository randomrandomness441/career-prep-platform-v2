## 1. Reframe the problem

[[021-dining-philosophers]] asked "can this deadlock" and answered it with lock ordering.
This question asks a completely different thing about the exact same table: given that
nobody deadlocks, can one participant still dominate the resource so completely that
everyone else is effectively locked out? `std::mutex` makes no promise about *who* gets a
lock next among several waiters, it's a correctness primitive, not a fairness one. A
philosopher that never waits between meals is, from the mutex's point of view, just a thread
that happens to show up to `lock()` very often; there is nothing in the primitive itself that
notices or cares that it's the same thread every time.

## 3. The broken version, first

The boilerplate is 021's own solution, unmodified, still correctly deadlock-free:

```cpp
if (philosopher == 0) { forks_[right].lock(); ...; forks_[left].lock(); ... }
else { forks_[left].lock(); ...; forks_[right].lock(); ... }
```

**Why it looks right:** it *is* right, for everything 021 asked. Nothing about fork ordering
has anything to do with fairness, this question's requirement is genuinely new, not a
regression of the old one, which is exactly why reusing a correct answer to a different
question is a natural mistake to make.

Measured directly: philosopher 0 calling `wantsToEat` back-to-back with zero delay,
neighbours "thinking" for a realistic 200µs between meals, 150ms window:

```
philosopher 0 ate 5603779 times; neighbours averaged 571.8 each -- a 9801x ratio
```

Nearly ten thousand times more meals than its neighbours' average, not "somewhat more,"
which asking more often would fairly earn, but complete effective exclusion of everyone
else. The mechanism: the instant philosopher 0 releases its forks, it immediately tries to
reacquire them; a neighbour that just went to sleep for 200µs has no chance of winning that
race in the meantime, over and over.

The fix: every philosopher backs off briefly, with randomized jitter, after eating and
before its next attempt:

```cpp
std::this_thread::sleep_for(std::chrono::microseconds(jitter_us(rng))); // 50-150us
```

Same measurement, same setup:

```
no fork ever shared between neighbours, and the hungry philosopher's advantage
over its neighbours stayed bounded, not runaway
```

(Measured separately for the specific numbers: philosopher 0 settles around 15,400 meals to
neighbours' ~600 each in the same window, still hungrier, as intended, but roughly 25x, not
9,800x.)

## 6. Where this solution fails

- **The jitter-avoids-synchronized-retries concern (the guide's follow-up #2: "if all
 philosophers use the exact same backoff, they might synchronize") is real in principle,
 and this course's own test of it did NOT reproduce it on this machine.** Testing a
 *uniform, fixed* (non-jittered) backoff applied to all five philosophers equally: the meal
 distribution came out essentially perfectly even (1172-1207 meals each across three
 repeats) with no sign of synchronized clustering or reduced throughput. Real OS scheduling
 jitter apparently desynchronizes threads enough on its own here, even with an identical
 nominal sleep duration, that the theoretical livelock this guide's follow-up warns about
 didn't show up. That doesn't mean the concern is wrong in general, it's a documented
 failure mode in other systems (classic Ethernet collision backoff is the canonical
 example), only that this specific test, on this specific machine, didn't demonstrate it.
 Jitter is kept in the solution anyway: it costs nothing here and is the safer default
 across schedulers this course hasn't tested on.
- **The backoff duration is a magic number, tuned to this test's specific think-time (200µs)
 and window (150ms).** A different ratio of hungry-caller aggressiveness to neighbour
 think-time would need a different backoff to land in a similarly "bounded, not runaway"
 place, this fix doesn't self-tune to the actual contention it's facing.
- **This fixes fairness for one aggressive caller against normal ones, it says nothing about
 N aggressive callers at once.** If two or more philosophers were simultaneously hungry with
 no think time, the backoff gives all of them the same head start against the genuinely
 idle ones, but doesn't arbitrate fairly *among* the aggressive callers themselves.
- **A backoff after every meal is a real latency cost paid by every philosopher, all the
 time, including the well-behaved ones who never needed it.** This is the general
 fairness-vs-throughput tradeoff: the guide's suggested alternative (a central "concierge"
 granting fork access by priority, eliminating both deadlock and starvation architecturally)
 avoids taxing every participant unconditionally, at the cost of a new bottleneck, every
 request now goes through one arbiter instead of acquiring forks directly.

## 7. Interview follow-ups

**"What's the absolute optimal fix, per the source material this course draws from?"** A
central concierge: philosophers request permission from one arbiter before touching any
forks, and the arbiter can grant that permission by priority (hungrier callers waiting
longer get bumped up, say) rather than by whichever thread happens to win a race on a
mutex. It eliminates both deadlock and starvation by construction, at the cost of every
request now serializing through one shared decision point, trading distributed,
lock-per-resource contention for one, more controllable bottleneck.

**"You measured that fixed backoff didn't cause the synchronized-retry problem here, does
that mean jitter is unnecessary busywork?"** Not necessarily, this measurement is one data
point on one machine's scheduler, and the failure mode it didn't reproduce is well-attested
elsewhere. The honest position, and the one worth taking into an interview: state what you
actually measured, state what it does and doesn't prove, and default to the safer practice
(jitter) when it's free, rather than either asserting a textbook risk you haven't verified or
dismissing it because you didn't happen to observe it once.

**"How would you detect this kind of starvation in a production system, where you can't just
add an instrumented meal counter?"** Track per-caller latency-to-service as a live metric,
if the same identity (thread, user, request source) shows up disproportionately in "served
quickly" versus other identities consistently showing "waited a long time," that's the
production signature of exactly this bug. It's the same idea as
[[041-deadlock-detection-watchdogs]]'s stall detection, aimed at a distributional unfairness
instead of an outright hang.
