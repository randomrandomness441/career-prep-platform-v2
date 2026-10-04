# A List You Can Walk While Someone Edits It

## ELI5: climbing a ladder, never letting go of one rung until you've got the next

Think about how you'd actually climb a ladder. You never let go of the rung you're
standing on until your other hand has a firm grip on the next one up. For a moment, both
hands are holding rungs at once. That overlap is exactly what keeps you from falling if
the ladder shifts underneath you.

That's this question, applied to a linked list instead of a ladder. If every reader had
to grab one single lock for the *entire* list just to walk it, only one person could ever
be climbing at a time. That defeats the point of a list many threads want to read and
edit concurrently. Instead, give every node (every rung) its own lock, and walk the list
hand-over-hand: hold the node you're on, grab the next one, *then* let go of the one
behind you. Two locks held at a time, never more, and never let go of the current rung
before you've got the next one.

## What you're actually building

Build a thread-safe singly linked list where **each node owns its own `std::mutex`**, so
that two threads can be at two different points in the list at the same time.

```cpp
class threadsafe_list {
public:
    threadsafe_list();
    ~threadsafe_list();
    threadsafe_list(const threadsafe_list&) = delete;
    threadsafe_list& operator=(const threadsafe_list&) = delete;

    void push_front(int value);

    void for_each(const std::function<void(int)>& f);

    std::optional<int> find_first_if(const std::function<bool(int)>& pred);  // first match, in order

    void remove_if(const std::function<bool(int)>& pred);                    // every match
};
```

**The technique, precisely.** A walk locks the node it's standing on, then locks the
*next* node, and only then releases the one it came from. While it holds node `A`,
nobody can rewrite `A->next`, so the pointer it's about to follow can't go stale. While
it holds node `B`, nobody can destroy `B`. Let go of `A` before taking `B`, and both of
those guarantees disappear. You've let go of the current rung with nothing in your other
hand yet.

## Requirements

1. Every node has its own `std::mutex`. There is no list-wide mutex.
2. A walk holds at most two node locks at once, and always acquires them in list order.
3. Use a **dummy head node** that never holds a value and is never removed. It's what
   makes `push_front` and "remove the first element" ordinary rather than special cases.
   Think of it as an imaginary rung below the first real one, so the first real rung has
   an ordinary neighbor too.
4. `for_each` calls `f` once per element, in list order, front to back.
5. `find_first_if` returns the value of the **first** element in list order that
   satisfies `pred`, or `std::nullopt`.
6. `remove_if` removes every element satisfying `pred` and destroys it.
7. `push_front` prepends. After pushing 0, 1, 2 a walk sees 2, 1, 0.

## Why the constraints exist

- **No shared/reader-writer mutex, no atomics, no deferred reclamation.** Plain
  `std::mutex` per node and plain destruction. The hand-over-hand technique itself is
  the whole exercise.
- **The destructor must not recurse once per element.** A chain of `unique_ptr`s
  destroyed by the default destructor blows the stack on a long list.
- **The list need not be safe to destroy while threads are still inside it.** Joining
  them is the caller's job.

**The question to answer before you write `remove_if`.** Unlinking node `B` means
writing `A->next`, and then destroying `B`. Which locks must you be holding at the
moment of the write, and which at the moment of the destruction? Getting this wrong
produces a list that passes a single-threaded test perfectly and hands a concurrent
walker a pointer into memory you've already freed. That's letting go of a rung a
half-second too early.
