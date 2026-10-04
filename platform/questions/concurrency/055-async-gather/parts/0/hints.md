"Gather results as they come in" is a natural way to read this problem, and it's the wrong
one — run the boilerplate's poll-each-future-round-robin approach against the tests and
watch the result order not match the input order, reliably, whenever tasks finish out of
sequence. The requirement is specifically about *input* order, not completion order.

---

`std::future<T>::get()` already blocks until *that specific future's* task is done — it
doesn't need any other future to be ready first. What does a loop that calls `futures[0].get()`,
then `futures[1].get()`, then `futures[2].get()`, in that exact order, actually do while
`futures[0]`'s task is still running?

---

It waits for exactly the right thing, in exactly the right order, with no polling at all:
call `.get()` on each future in input order, and each call blocks precisely as long as that
one task takes — meanwhile any *other* task that finishes first has simply finished; nothing
is lost by not looking at it yet. There's no clever synchronization to add here; the
"trick" is recognizing that a plain sequential loop already has the property you need.
