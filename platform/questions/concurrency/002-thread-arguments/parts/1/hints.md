The thread reads `label` after `make_labeler` has returned. Where does `label` live, and
who is still keeping it alive at that moment?
---
Nobody. It was a local; its storage was reclaimed when the function returned. Capturing it
by reference stores a pointer into a dead stack frame.

The closure must **own** the data it needs, rather than pointing at someone else's.
---
Change the capture from `[&label, ...]` to `[label, ...]`. That copies the string into the
closure, which lives inside the `std::thread` object and survives as long as the thread.

For a large or move-only payload, capture by move instead:
`[label = std::move(label), out, go]`.
