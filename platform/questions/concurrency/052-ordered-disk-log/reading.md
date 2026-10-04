## 1. Reframe the problem

"Write safely from many threads" and "write *in order* from many threads" sound like the
same problem wearing different words, but they're not, mutual exclusion is about *what
happens while you hold the lock*; order is about *who gets the lock next*, and a mutex makes
no promise about the second question at all. The fix isn't a better mutex, it's a different
piece of state entirely: instead of "may I write," each thread needs to ask "is it my turn,"
and a mutex has no concept of turns. This is a ticket lock, the same idea a deli counter or
a phone queue uses, and it's the direct answer whenever "safe" isn't a strong enough
guarantee and "in the order requests arrived" actually matters.

## 3. The broken version, first

The boilerplate assigns a real, correct ticket, then just locks a mutex to write:

```cpp
std::uint64_t ticket = next_ticket_.fetch_add(1, std::memory_order_relaxed);
std::lock_guard<std::mutex> lk(m_);
entries_.push_back(std::move(record));
```

**Why it looks right:** the ticket genuinely is correct and atomic, there really is a
well-defined "who asked first." It's easy to read "assign a ticket" plus "protect the write
with a lock" as the whole solution, because both pieces are individually correct; the gap is
that nothing connects the ticket's value to when the lock is actually granted.

Running it, 16 threads, 500 writes each:

```
log entry 119 is "t1_1", expected "t0_118" (the record that was assigned
ticket 119) -- the log is not in ticket order
```

Not a rare misordering, in a quick empirical check, over 95% of entries landed in the wrong
slot. `std::mutex` on this platform makes no attempt at FIFO fairness; a thread reacquiring
the lock it just released is common, not exceptional.

The fix makes each thread wait for its own specific turn:

```cpp
cv_.wait(lk, [&] { return ticket == next_to_write_; });
entries_.push_back(std::move(record));
++next_to_write_;
```

Same 16-thread, 500-write test: exact ticket order, every run.

## 6. Where this solution fails

- **Every write now serializes completely, there is no concurrency left in the write path
 at all**, by design: enforcing a strict global order on independent writers means none of
 them can genuinely overlap, since "who's next" is a single, shared piece of state every
 writer contends on. This is the direct cost of the ordering guarantee, not an
 implementation flaw, any design that guarantees total order pays it.
- **A writer that's descheduled (or crashes) right after taking its ticket blocks every
 writer with a later ticket, forever.** There's no way for ticket 47 to write until ticket
 46 has written, no matter how long ticket 46's thread takes (or whether it comes back at
 all), the same hazard [[044-robot-grid-locking]]'s reading raises for a stalled lock
 holder, here applied to an entire ordered queue instead of one resource.
- **This is an in-memory stand-in for "disk."** A real disk-backed log adds its own
 ordering concerns this exercise doesn't touch: does the OS or filesystem reorder or buffer
 writes before they're actually durable, and does a crash after `write_record` returns but
 before an `fsync` lose data the caller believed was safely ordered and persisted?
- **Tickets are dense and start at 0, which this design relies on** (`next_to_write_`
 advances by exactly one per completed write), a design that needed to skip or cancel
 tickets (a writer that takes a ticket and then decides not to write after all) would stall
 every later ticket forever, since nothing here can "expire" or "consume" a ticket that
 never gets used.

## 7. Interview follow-ups

**"Why not just use a std::priority_queue keyed by ticket instead of blocking each thread on
its own turn?"** That's a legitimate alternative shape: every writer pushes its
(ticket, record) pair into a shared min-heap immediately (no waiting to enqueue), and a
*separate* consumer thread pops and writes whenever the heap's minimum ticket equals
`next_to_write_`. It decouples "produce" from "the moment of writing" more explicitly, at
the cost of an extra data structure and a dedicated writer thread, worth it when producers
shouldn't block at all, even briefly, waiting for their turn.

**"What does 'durable, ordered' actually require on real hardware, beyond what this exercise
covers?"** An `fsync`/`fdatasync` call after each write (or a batch of writes) before
acknowledging it as complete, without that, the OS page cache can hold data that hasn't
reached the physical disk, and a crash can lose "already written" records or reorder what
actually lands on stable storage relative to what the application believed was already
durable.

**"10^8 writes/sec target, this design obviously can't hit that, since every write
serializes. What would you actually build?"** Batch: let each writer accumulate several
records locally, then claim one ticket per *batch* instead of per record, writing many
records per turn. This trades strict single-record ordering granularity for far less
contention on the shared "whose turn" state, the same batching idea
[[045-llm-batch-dispatcher]] uses for a different bottleneck (a model's forward pass instead
of a disk write), reapplied here.
