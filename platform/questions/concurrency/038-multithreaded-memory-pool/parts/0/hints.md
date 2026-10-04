Run the boilerplate against the tests before changing anything — the free list itself
(guarded by the mutex) is correct; capacity, exhaustion, and double-free rejection all pass.
It's `blocks_in_use()` that comes back wrong, and only under real concurrent load. What's
different about how that counter is updated compared to everything else in the class?

---

`++in_use_count_` and `--in_use_count_` happen *outside* the mutex, on a plain `std::size_t`.
Under concurrent calls from multiple threads, that's a read-modify-write with no exclusion at
all: two threads can both read the same old value, both compute `old + 1`, and both write it
back — one increment is lost. "It's just a stats counter, an `int++` is basically free" is
the exact reasoning that made this look safe to skip locking. Making it `std::atomic<size_t>`
(`fetch_add`/`fetch_sub`) is the minimal fix and is enough to pass every test here.

---

If you want to go further: this question's own authoring tried dropping the mutex entirely
and making the free list lock-free, the same intrusive-linked-list-via-CAS-retry-loop
technique as [[015-treiber-stack]], with a generation-tagged head to close the ABA problem
that recycling creates (read 015's reading if that term is new). It compiled, passed every
correctness run — and ThreadSanitizer still found a real data race in it. The reading walks
through exactly why a tagged CAS alone isn't enough here, and it's a genuinely instructive
attempt to try and watch fail, but it is not the shipped solution for a reason: the mutex is
the correct answer this question actually wants.
