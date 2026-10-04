## 1. Reframe the problem

"Make this map thread-safe" has an obvious answer: wrap it in a mutex. That answer is
correct and it is why your service does not scale.

A mutex is not just protection, it is **serialisation**. Every thread that takes it waits
for every other thread that holds it, whether or not they had anything to do with each
other. Two threads looking up two completely unrelated keys take turns for no reason. And
if the map is read-heavy, the common case for caches, config, routing tables, you have
made readers, who cannot interfere with each other at all, queue up single file.

So the real question is: **how do I make threads that cannot possibly conflict stop
waiting for each other?** Two independent answers, and this question uses both.

1. **Split the lock.** If the data is partitioned into N buckets with N separate locks,
 two threads touching different buckets never meet.
2. **Distinguish readers from writers.** Many readers can safely share the same data at
 once. Only writers need exclusivity.

## 2. The tools

### `std::shared_mutex`, one writer or many readers

```cpp
#include <shared_mutex>

std::shared_mutex m;

// reader: many of these can hold the lock simultaneously
{
    std::shared_lock<std::shared_mutex> lk(m);
    return data.find(key);
}

// writer: excludes everyone, readers and writers alike
{
    std::unique_lock<std::shared_mutex> lk(m);
    data[key] = value;
}
```

Note the two lock types. `shared_lock` takes it in shared mode, `unique_lock` (or
`lock_guard`) in exclusive mode. Using the wrong one is silent, `lock_guard` on a
`shared_mutex` compiles fine and simply serialises your readers, undoing the whole point.

### Bucketing

```cpp
struct bucket {
    std::list<std::pair<Key, Value>> data;
    mutable std::shared_mutex mutex; // one lock per bucket
};
std::vector<std::unique_ptr<bucket>> buckets;

bucket& bucket_for(const Key& k) {
    return *buckets[hasher(k) % buckets.size()];
}
```

Every operation hashes the key, finds its bucket, and locks only that bucket. Two threads
on different buckets are completely independent.

**The bucket count is fixed at construction and never changes.** That is a deliberate
design decision, not laziness: growing the table would require locking *every* bucket at
once and rehashing everything, which serialises the whole structure and destroys the
property you built it for. If you need growth, you build a different structure.

Because the count is fixed, the reference returned by `bucket_for` stays valid forever, so
you can take it without holding any lock at all.

### Return by value, not by reference

```cpp
Value value_for(const Key& key, const Value& fallback) const {
    std::shared_lock<std::shared_mutex> lk(b.mutex);
    auto it = find_entry_for(key);
    return it == b.data.end() ? fallback : it->second; // a COPY
}
```

Returning a reference would hand the caller a pointer into the table that outlives the
lock, another thread could erase the entry a nanosecond later. The copy happens while the
lock is held, which is what makes it safe. This is the same lesson as the interface-race
question: **the API must not let a value escape its protection.**

## 3. The broken version, first

```cpp
std::map<Key, Value> data;
mutable std::mutex mutex;

Value value_for(const Key& key) const {
    std::lock_guard<std::mutex> lock(mutex);
    ...
}
```

Thread-safe. Correct. Measured against the bucketed version, 200,000 lookups per thread:

```
 threads single mutex per-bucket shared_mutex speedup
 1 9.6 ms 6.1 ms 1.57x
 2 30.8 ms 16.7 ms 1.85x
 4 65.0 ms 23.6 ms 2.75x
 8 142.8 ms 35.9 ms 3.97x
 10 161.7 ms 35.0 ms 4.63x
```

Read the single-mutex column downwards: 9.6 → 30.8 → 65.0 → 142.8 → 161.7. **Adding threads
makes the total time worse roughly in proportion.** Ten threads do not finish ten times
sooner; they take turns, and then pay contention on top. You bought ten cores and used one,
badly.

Now the bucketed column: 6.1 → 16.7 → 23.6 → 35.9 → 35.0. It grows far more slowly and
flattens out near the core count. That flattening is what "scales" actually looks like.

There is no correctness bug here. Both versions return the right answers forever. This is a
design failure, which is why it survives code review and shows up as a mystery in
production dashboards.

## 4. Real-world usage

**Why reader-writer locks exist.** Courtois, Heymans and Parnas described the readers–
writers problem in 1971, and the observation behind it is simply that "mutual exclusion" is
stronger than most data actually needs. Reads do not conflict with reads. A lock that
cannot express that forces every reader to behave like a writer. C++ was late to this,
`std::shared_mutex` only arrived in C++17, with `shared_timed_mutex` in C++14; before that
everyone used `pthread_rwlock_t` or Boost.

**Why bucketing exists.** Java's `ConcurrentHashMap` (2004) popularised lock striping, and
essentially every production concurrent map since is a variation: `folly::ConcurrentHashMap`,
Intel TBB's `concurrent_hash_map`, Rust's `dashmap`. The insight is that a hash function
already partitions your keys, so use that partition for locking too, and you get
concurrency for free from work you were doing anyway.

**Where you meet it:**

- **Caches.** Overwhelmingly read-heavy, which is precisely where a global mutex hurts most.
- **Config and feature flags.** Read on every request, written once a minute. Textbook
 `shared_mutex`.
- **Routing tables, session stores, connection registries**, any lookup structure on a hot
 path in a multithreaded server.

**Where NOT to use it:**

- **Not when reads are trivial and rare.** `shared_mutex` is more expensive than `mutex`
 per operation: it must track a reader count atomically. For a table read a few times a
 second, or one guarding a single `int`, a plain mutex is faster and simpler.
- **Not for write-heavy workloads.** If most operations write, shared mode never engages and
 you pay the extra bookkeeping for nothing. Measure the read/write ratio before choosing.
- **Not when the map must grow.** Fixed bucket count is load-bearing. A resizable concurrent
 map is a genuinely harder structure.
- **Not before you have measured.** A global mutex is fine at low thread counts and is much
 easier to reason about. The table above shows the crossover is real, but it is also only
 ~1.6x at one thread, the win comes from concurrency, not from the structure itself.

## 5. Performance

The scaling table above is the headline. Two more things it took a mistake to learn.

**Bucket count is not a detail.** My first run of that benchmark used the default of 19
buckets with 5,000 entries, and produced this:

```
 threads single mutex per-bucket shared_mutex speedup
 1 9.0 ms 73.4 ms 0.12x
 8 144.0 ms 343.9 ms 0.42x
```

The "optimised" version was **eight times slower**. With 19 buckets and 5,000 entries, each
bucket held ~263 entries in a `std::list`, and every lookup walked it linearly, ~263
comparisons against `std::map`'s ~12. The locking improvement was real and completely buried
by an O(n) search inside each bucket.

Raising the count to 1,009 (≈5 entries per bucket) produced the good table. **Size buckets
so the per-bucket list stays short, a handful of entries.** A concurrent hash map with too
few buckets is a concurrent linked list.

**Watch out for false sharing between buckets.** Adjacent `bucket` objects can land on the
same 64-byte cache line, so two threads locking two different buckets still bounce the same
line between cores. Storing buckets as `unique_ptr` (separate heap allocations, as the
solution does) sidesteps this; a `vector<bucket>` of small buckets would not. `alignas(64)`
on the bucket type is the explicit fix.

## 6. Where this solution fails

- **Fixed bucket count.** Sized for 1,000 entries and given 1,000,000, it degrades to linear
 search per bucket, exactly the failure measured above, just slower to notice.
- **A bad hash collapses it.** All keys hashing to one bucket gives you a single mutex and a
 linked list, i.e. worse than where you started. `std::hash<int>` on many standard libraries
 is the identity function, so keys that are all multiples of the bucket count land in one
 bucket. Real deployments have been taken down by exactly this.
- **`value_for` returns a copy.** Correct, but expensive for large values. Returning
 `shared_ptr<const Value>` avoids the copy while keeping the value alive past the lock.
- **No atomic multi-key operations.** "Move a value from key A to key B" cannot be done
 atomically here without locking two buckets, and doing that reintroduces the deadlock
 problem from the `scoped_lock` question. The API deliberately offers no such operation.
- **Iteration is a lie or a stall.** A consistent snapshot requires locking every bucket at
 once, which serialises everything. Iterating without that gives you a view that never
 existed at any single instant. Williams' version locks all buckets for `get_map()`; know
 which one you are getting.
- **Writer starvation is possible.** Under a constant stream of readers, a waiting writer
 may never acquire exclusive access. Whether it does depends entirely on your
 implementation's fairness policy, which the standard does not specify.

## 7. Interview follow-ups

**"You measured the 'optimised' bucketed version being 8x SLOWER than a single mutex at
first, what went wrong, and what does that teach about optimizing concurrent structures in
general?"** 19 buckets, 5,000 entries, ~263 entries per bucket in a `std::list` walked
linearly, the locking improvement was real, but completely buried under an O(n) search
replacing `std::map`'s O(log n). The lesson: a concurrency fix and a data-structure fix are
two different axes, and improving one while accidentally regressing the other can net out
negative. Always measure the whole change, not just reason about the piece you intended to
improve, this is the same discipline [[013-store-buffering]] and
[[040-lock-free-hashmap]]'s readings apply to their own measured surprises.

**"A bad hash function sends every key to the same bucket. What's the actual failure mode,
and is it just a performance problem?"** It's a correctness-adjacent collapse, not a crash:
every operation ends up serialized behind one bucket's single mutex, and that bucket's
internal list degrades to a full linear scan, you're left with strictly worse performance
than the single-mutex baseline you were trying to improve on, since now you pay both the
bucketing overhead AND get none of its benefit. `std::hash<int>` being the identity function
on some standard libraries means keys that are multiples of the bucket count all collide,
this has genuinely taken down real deployments, which is why the reading calls it out by
name rather than as a theoretical concern.

**"Small machine, 2 cores. Does bucketing with shared_mutex still pay for itself over a
plain mutex?"** The scaling table shows the crossover is real even at 1 thread (~1.57x,
6.1ms vs 9.6ms), but the reading is explicit that the *win comes from concurrency*, not the
structure alone, and with only 2 cores there's far less concurrency to extract than the
10-core table shows. At 2 threads specifically the measured gap was already 1.85x, so it
still pays off, but the margin narrows the fewer cores are actually available to exploit
the reduced contention.

**"10^8 lookups/sec target on this structure, what's the first thing that breaks?"** Bucket
count sized for the wrong entry count, exactly as measured: a table sized for 1,000 entries
handling 1,000,000 degrades every bucket to the same O(n) linear-scan disaster the reading's
own accidental first benchmark hit. At extreme lookup rates, `shared_mutex`'s own
atomic-reader-count bookkeeping (real overhead the reading names explicitly as the reason
not to reach for it when reads are trivial) also becomes a genuine cost per operation, not
just a rounding error, worth re-measuring at that scale rather than assuming the 10-thread
numbers here extrapolate linearly.

**"A caller needs to atomically move a value from key A to key B, your API has no such
operation. How would you support it without breaking the whole design?"** You can't add it
as a bucket-internal operation, because A and B likely hash to different buckets, and locking
two buckets together reintroduces exactly the AB/BA deadlock risk [[005-scoped-lock]]'s
reading covers, two threads moving in opposite directions (A→B and B→A) could deadlock
under naive two-bucket locking. The fix is the same one: use `std::lock()`'s deadlock-avoiding
multi-lock acquisition across the two specific buckets' mutexes for that one composite
operation, rather than adding a bespoke method that assumes single-bucket locking is enough.
