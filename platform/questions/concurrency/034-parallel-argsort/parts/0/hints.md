Sequential counting sort has three steps: count how many of each value there are, turn
those counts into starting offsets (a prefix sum), then place each element at its bucket's
next free offset. Parallelizing the *last* step is the easy part — once you know each
bucket's starting offset, one atomic counter per bucket, incremented via `fetch_add`, lets
every thread safely claim the next slot for whatever value it's currently placing. The first
step — counting — is where it's tempting to reach for the same trick, and where this
question's actual lesson lives.

---

Run the boilerplate against the tests before assuming a shared histogram array is fine —
the counting pass there has every thread do `++count[bucket]` on one array all threads
share. That's a read-modify-write with no exclusion: two threads counting the same bucket
value at nearly the same moment can both read the same old count and both write back the
same incremented value, losing one count. Under real load, with hundreds of buckets and
several threads, this loses hundreds of counts every run — enough that the resulting
`perm` isn't even a valid permutation anymore.

---

Give each thread its own **local** histogram — a plain `std::vector<std::size_t>` that only
that thread ever touches, no synchronization needed at all — then merge the per-thread
histograms into one global histogram with a short, sequential loop once every thread is
done counting. The merge is cheap (`num_threads * num_buckets` additions, not
`n` operations), and it means the expensive part — scanning all `n` elements — never touches
shared, contended memory during the counting phase at all.
