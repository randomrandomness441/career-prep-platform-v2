# Backpressure: What to Do When the Consumer Is Slower

## ELI5: an inbox tray that fills up faster than you can read it

You have a physical inbox tray on your desk. Mail keeps arriving. Some days you read it
as fast as it comes in, fine. Other days, mail arrives faster than you can possibly
read it, and the tray is going to overflow eventually, on someone's worst day, guaranteed.
You have exactly three honest choices when that happens:

- **Tell the mail carrier to wait** at the door until you've cleared some space. Nothing's
 lost, but the carrier (the producer) is now stuck standing there.
- **Refuse the new letter, leave it undelivered.** The tray stays as it was; whatever just
 arrived is simply dropped.
- **Make room by throwing out your oldest unread letter**, and take the new one instead.
 You're betting the newest information matters more than the oldest.

What you must never quietly do is just... keep stacking letters higher and higher,
forever, with no plan. That's not a strategy, that's the tray collapsing off the desk
with no warning, which is exactly what an unbounded queue does to a process's memory.

## What you're actually building

Finish `backpressure_queue<T>`, a queue where the capacity is a promise about memory,
and the policy is a promise about what happens when that promise can't be kept:

```cpp
enum class overflow_policy {
    Block, // the producer waits until there is room
    DropNewest, // refuse the arriving item, keep the backlog, push returns false
    DropOldest, // evict the head, keep the arriving item, push returns true
};

backpressure_queue<int> q(capacity, policy);
bool push(T v); // Block: true (after waiting); drops: false iff refused
std::optional<T> pop(); // blocks while empty; nullopt once closed AND drained
std::optional<T> try_pop(); // never blocks
void close(); // wake everyone; blocked pushes stop accepting
std::size_t size() const; // never exceeds capacity
std::size_t dropped() const; // every discarded item, counted
```

## Where the naive version breaks

The naive version compiles, passes every happy-path test, and is an **unbounded queue**:
`push()` never blocks, never drops, never fails, and a producer that outruns its
consumer grows the process until the OOM killer ends it, with no warning in any log. The
tray, collapsing off the desk, silently.

## Your task

Fill in the four TODOs:

1. **Enforce the bound**, `capacity_` is stored and then never actually consulted.
2. **Implement the three policies** exactly as specified above.
3. **Count every discarded item in `dropped()`.** An unreported drop is data loss; a
 counted drop is a metric you can alert on.
4. **Making room is news.** `pop()` must wake a producer blocked on a full queue, that
 means a second condition variable (`not_empty_` alone can't say *why* it's waking you).

**Pass = CLEAN**: capacity respected, each policy's semantics exact (which items
survive, what `push` returned, what `dropped()` says), blocked producers woken by pops
and by `close()`, through correctness + ThreadSanitizer + 150 shaken stress runs.
