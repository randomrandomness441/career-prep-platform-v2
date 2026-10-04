## 1. Reframe

Not every full-JVM freeze is garbage collection. The safepoint mechanism GC uses is a
general-purpose whole-JVM coordination point, and other subsystems — biased lock
revocation among them — reused it for operations that have nothing to do with memory
management.

## 3. The broken version, first

An engineer debugging unexplained pauses on an older JDK checks GC logs first (the
default instinct — question 004 already covers why GC is the natural first suspect for
any pause), finds nothing correlating, and concludes the pauses are unexplainable or
gives up looking for a cause. The actual cause was never going to show up in a GC log,
because it was never a GC event — it was a completely different subsystem borrowing the
same freeze-everything mechanism for its own, unrelated purpose.

## 4. Interview follow-ups

- If a team on an affected JDK version genuinely can't upgrade soon, is there a
  mitigation short of disabling biased locking entirely? `-XX:-UseBiasedLocking`
  disables it JVM-wide, trading away its benefit (near-free locking for genuinely
  single-threaded-owned locks) everywhere in exchange for never paying the revocation
  cost anywhere — a real tradeoff, not a free win, worth measuring against the specific
  workload before flipping it.
- Why was biased locking worth having in the first place, given this cost? For the
  common case it targeted — a lock object that really is only ever touched by one thread
  for its whole life — it removed real, measurable per-acquisition overhead compared to
  even an uncontended normal lock. The optimization was a good bet for the common case;
  the revocation cost is the price paid specifically when that bet turns out wrong,
  which is the same "speculative optimization, real cost when the assumption breaks"
  shape as question 014's deoptimization, just at the locking-subsystem level instead of
  the JIT's.
