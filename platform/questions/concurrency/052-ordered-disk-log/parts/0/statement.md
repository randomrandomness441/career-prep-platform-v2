# Ordered Disk Log

## ELI5: a deli's "take a number" dispenser

At a busy deli counter, you don't get served based on who physically shoves to the front
fastest, you pull a numbered ticket from the dispenser the instant you walk in, and
you're served strictly in ticket order. Ticket 7 is served after ticket 6, always, even
if the person holding ticket 7 happens to elbow their way to the counter first. The
*ticket number* defines the real order; who arrives at the counter when is irrelevant.

This question is that dispenser, applied to a log: many threads append records "at the
same time," but recovery later needs to read them back in one specific, well-defined
order, not whatever order the OS happened to schedule the writing threads in.

## What you're actually building

```cpp
class OrderedLog {
public:
    // Returns the ticket this call was assigned.
    std::uint64_t write_record(std::string record);
    std::vector<std::string> contents() const; // everything written, in log order
};
```

"Well-defined order" is made precise with a **ticket**: each call to `write_record` is
assigned a ticket, in a real, total order (two concurrent calls can never get the same
ticket, and there's always a definite first and second). The log's contents must end up
in **ticket order**, `contents()[k]` must be the record that was assigned ticket `k`,
no matter which thread's write physically happens first in real time.

## Requirements

1. Tickets are assigned atomically and are dense, starting at 0.
2. `contents()` reflects every completed `write_record` call, in strict ticket order.
3. Correct under many threads calling `write_record` concurrently and repeatedly.

## Why the constraints exist

**Getting mutual exclusion right (no torn writes) is necessary but not sufficient**,
the requirement is about *order*, not just safety. A dispenser that hands out valid,
non-duplicate tickets but then serves people in a random order has only solved half the
problem.
