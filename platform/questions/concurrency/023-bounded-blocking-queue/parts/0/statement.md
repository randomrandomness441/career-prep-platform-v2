# A Bounded Blocking Queue

## ELI5: the kitchen pass-through shelf

Picture a tiny pass-through shelf between a kitchen and the dining room. It only holds
3 plates. Cooks finish a dish and set it on the shelf. Waiters grab plates off the shelf
and carry them to tables.

- **The shelf is full?** A cook has to wait, there's nowhere to put the next plate,
 until a waiter clears one off.
- **The shelf is empty?** A waiter has to wait, there's nothing to grab yet, until a
 cook drops one off.
- **Nobody stands there asking "anything yet? anything yet?"** over and over. That would
 burn energy for nothing, you just wait until someone taps you on the shoulder.
- **The trap:** get the shelf's "traffic control" wrong and, with a busy enough kitchen,
 cooks and waiters can all end up asleep at once, each waiting for someone who's also
 asleep. The shelf just... stops. That's the bug this question is really about.

*(This is LeetCode 1188, "Design Bounded Blocking Queue", we're adding the part LeetCode
quietly skips: what happens when the kitchen closes for the night while people are still
standing at the shelf.)*

## What you're actually building

```cpp
class bounded_queue {
public:
    explicit bounded_queue(std::size_t capacity);

    bounded_queue(const bounded_queue&) = delete;
    bounded_queue& operator=(const bounded_queue&) = delete;

    // Drop off a plate. Blocks while the shelf is full.
    // Returns false if the kitchen closed instead of making room.
    bool enqueue(int v);

    // Grab a plate. Blocks while the shelf is empty.
    // Returns nullopt only once the kitchen is closed AND the shelf is empty.
    std::optional<int> dequeue();

    // Same as above, but never wait -- give up immediately instead.
    bool try_enqueue(int v);
    std::optional<int> try_dequeue();

    // Closing time. Wakes everyone currently waiting at the shelf. Safe to call twice.
    void close();

    bool is_closed() const;
    std::size_t size() const;
    std::size_t capacity() const;
};
```

**The hard requirement:** any mix of cooks and waiters has to keep moving, forever. It's
easy to write a shelf that works fine with one cook and one waiter and then silently
deadlocks, everyone asleep, nobody left to wake anyone up, the moment you have several
of each. That failure mode is invisible until you specifically go looking for it.

## Requirements

1. **First plate in, first plate out**, FIFO order.
2. **The shelf never holds more than `capacity()` plates**, no matter how many cooks are
 pushing at once.
3. `enqueue` waits while the shelf is full; `dequeue` waits while it's empty. No
 spinning, a waiting thread actually sleeps, it doesn't loop-and-check.
4. Every plate a cook drops off gets picked up by **exactly one** waiter. Never two,
 never zero.
5. **This is the one that trips people up.** Four cooks and four waiters at a shelf that
 holds only 2 plates must all keep moving. It's easy to write a version where, under
 the wrong mix of cooks and waiters, everyone quietly falls asleep and nobody ever
 wakes back up. Write the version you'd naturally write first, run it, and watch that
 happen.
6. **Closing time.** `close()` wakes up everyone currently waiting at the shelf. A waiter
 who was waiting for a plate that's never coming hears "nothing's coming" (`nullopt`).
 A cook who was waiting for room hears "give up" (`false`).
7. Closing the kitchen doesn't throw away plates already sitting on the shelf, waiters
 still get to grab those. Only once the shelf is closed *and* empty does a waiter hear
 "nothing's coming."

## Why the constraints exist

- **`std::mutex` and `std::condition_variable` only, no lock-free tricks.** This
 question is specifically about getting condition variables right, not about atomics.
- **Wait with the predicate overload, `cv.wait(lk, pred)`, never the bare `cv.wait(lk)`.**
 The bare form wakes up on *any* signal, including ones meant for someone else, and
 can't tell "I was woken for a real reason" from "I was woken for nothing." The
 predicate form loops that check for you.
- **Watch your types:** `q_.size()` is `std::size_t`. Store the capacity as an `int` and
 the comparison `q_.size() < capacity_` becomes a signed/unsigned comparison, which the
 harness's `-Wall -Wextra` will refuse to compile.

In short: this question is "implement a bounded producer/consumer queue with condition
variables", but requirement 5, the deadlock under real contention, is the whole point.
Everything else here is bookkeeping around it.
