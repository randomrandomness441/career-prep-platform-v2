# Thread-Local Storage

## ELI5: every barista gets their own personal order pad

A coffee shop has several baristas working at once. Each one has their own personal
order pad, numbering orders 1, 2, 3... on their own pad, independently. Barista A's
order #1 has nothing to do with barista B's order #1, they're different pads. But
there's also a shared staff-badge box at the door: the first time any barista clocks in,
they draw the next badge number from that one shared box, and that badge number is
theirs for the whole shift.

That's the difference this question is built on: some state genuinely belongs to *the
shop* (the shared badge box, one dispenser, everyone draws from it), and some state
genuinely belongs to *each barista individually* (their own order pad, nobody else ever
touches it, and it doesn't need to be guarded from anyone, because nobody else can reach
it).

## What you're actually building

A server dispatches work to worker threads. Every thread must number its own work: a
per-thread ticket counter that runs 1, 2, 3, ... for that thread alone, and a per-thread
station identity assigned from a shared dispenser the first time the thread asks for it.
The interface is static functions, callable from any function running on the thread,
with no handle to carry around.

```cpp
class ticket_station {
public:
    static long next_ticket(); // 1, 2, 3, ... for THIS thread, in order, no gaps
    static long issued_here(); // how many tickets THIS thread has issued so far
    static int station_id(); // unique id for THIS thread, assigned on first use
};
```

## Requirements

1. `next_ticket()` returns this thread's own sequence, 1, 2, 3, ..., no matter what any
 other thread does concurrently. A thread that starts later starts from 1 again, the
 state belongs to the thread, not the process. (Its own order pad, not the shop's.)
2. `issued_here()` counts only this thread's tickets.
3. `station_id()` returns a unique id per thread: the first call takes the next number
 from the shared dispenser (0, 1, 2, ...), and the answer is stable for the thread's
 whole life afterward. (The shared badge box, drawn from exactly once per barista.)
4. **Ticket issuing must not serialize threads:** no mutex, no atomic on the counter's
 hot path. Eight threads issuing tickets at once shouldn't contend at all, they're
 each writing on their own pad.

## Why the constraints exist

- **The tests hammer all three methods from many threads simultaneously.**
- **Some state here is genuinely per-process (the dispenser), and some is genuinely
 per-thread (the ticket counter).** The whole exercise is telling them apart correctly,
 using storage duration.
- **No passing a counter object around, no thread-id-keyed maps.** Those would work, but
 they dodge the actual lesson, the fix belongs in *how the state is declared*, not in
 extra bookkeeping layered on top.
