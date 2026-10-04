## 1. Reframe the problem

A plain `std::mutex` makes an implicit assumption: every critical section is equally
dangerous, so every thread that wants in has to wait for every other thread, no matter what
either of them is actually doing. That assumption is wasteful for a very common shape of
workload, state that's read constantly and written rarely, because two threads that are
both only *reading* can never actually interfere with each other. There's nothing to
protect them from; the danger is only ever a read overlapping a write, or a write overlapping
another write.

`std::shared_mutex` gives you two lock modes instead of one, so you can say which kind of
access this call is: `std::shared_lock` ("I'm only reading, let other readers in too") and
`std::unique_lock` ("I'm about to mutate, nobody else, reader or writer, gets in until I'm
done"). The correctness rule is exactly the rule you'd guess: pick the mode by what the
function actually does to the data, not by "reads are common so make everything shared for
speed", that second instinct is precisely this question's bug.

## 3. The broken version, first

The boilerplate uses `std::shared_lock` in *both* `get()` and `set()`:

```cpp
void set(std::string key, std::string value) {
    std::shared_lock<std::shared_mutex> lk(m_); // wrong mode
    data_[std::move(key)] = std::move(value);
}
```

**Why it looks right:** `std::shared_mutex` reads as "the fast one for read-heavy code," and
if you're optimizing with half an eye on that reputation, reaching for `shared_lock`
everywhere can look like the performance-conscious choice rather than a correctness bug,
especially since a single-threaded smoke test of `set()` followed by `get()` passes
without incident; the bug only exists when two `set()` calls are actually concurrent.

Running it, 8 writer threads inserting 2,000 distinct keys each, 4 reader threads running
throughout:

```
TIMED OUT -- the program stopped making progress
```

`std::shared_lock` allows many holders at once, by design, so multiple `set()` calls run
*simultaneously*, each believing it has exclusive access, each mutating the same
`std::unordered_map`. Rebuilding with ThreadSanitizer instead of the plain build shows
exactly what that costs:

```
WARNING: ThreadSanitizer: data race
 Write ... by thread T4: __hash_table::__emplace_unique_key_args ... ConfigStore::set
 Previous read ... by thread T3: __hash_table::__do_rehash ... ConfigStore::set
```

Two threads are inside `std::unordered_map`'s internals at the same time, one rehashing the
table while another reads the bucket structure mid-rehash. `unordered_map` was never designed
to tolerate concurrent mutation, even to different keys; an insert can trigger a rehash that
touches the whole table, not just the one bucket the new key lands in. On this run it
manifests as a hang (a corrupted bucket chain forming a cycle); on a different schedule it
could just as easily be a crash or silent corruption.

The fix is one word per function: `std::unique_lock` for `set()`. Same test:

```
8 writers x 2000 keys, 4 concurrent readers: every key intact, no corruption
```

## 6. Where this solution fails

- **`std::shared_mutex` is genuinely more expensive per operation than `std::mutex`, and this
 is not close on this machine.** Measured here, single-threaded, uncontended: plain mutex
 lock/unlock **~10.6 ns**, `shared_lock`/unlock on a `shared_mutex` **~22.6 ns**, roughly
 2x, because a shared mutex has to track a reader count atomically even for the "cheap"
 path. Under 8 concurrent readers doing a trivial read (one word), plain `std::mutex`
 serializing all 8 (**~25.5 ns/read**) beat `shared_mutex` letting all 8 run concurrently
 (**~42.8 ns/read**), the parallelism `shared_mutex` buys didn't cover its own overhead
 for work this cheap. Widening the "read" to 200 words of real work didn't flip the result
 either (mutex ~60.5 ns/read vs shared_mutex ~79.8 ns/read, still on this machine, still 8
 readers), this course did not find the crossover point where `shared_mutex` actually wins
 within the range tested; assume it exists for large enough read payloads or rare enough
 writes, but don't assume `shared_mutex` is free just because reads dominate.
- **Writer starvation is a real risk *in principle*, but this course could not reproduce it on
 this machine.** The classic concern: if readers keep arriving, a waiting writer can be
 perpetually deferred, since `shared_lock` holders never conflict with each other. Tested
 directly here, 8 threads doing nothing but acquiring and releasing `shared_lock` back to
 back (900,000+ reads in the measurement window) while one writer waited for a
 `unique_lock`, **the writer got in in ~0.04 ms every time**, essentially unaffected by the
 continuous read traffic. `std::shared_mutex` does not *promise* writer priority by
 standard, so a different standard library implementation is free to behave differently,
 but on this machine's libc++, the starvation scenario the guide describes did not manifest.
 Don't take either result, "it starves" or "it's fine", as a portable guarantee; measure
 on the implementation you actually ship on if this matters to you.
- **Every `get()` still touches one shared atomic reader count**, even though logically
 independent readers of different keys have nothing to do with each other, `shared_mutex`
 makes concurrent readers *safe*, not *contention-free*. A design with per-shard locks (one
 `shared_mutex` per bucket of keys, not one for the whole map) removes that remaining shared
 state entirely, at the cost of real complexity.

## 7. Interview follow-ups

**"You measured shared_mutex losing to a plain mutex twice, when does it actually win?"**
When the *held* time under the lock is large enough that concurrent execution meaningfully
overlaps, many readers doing real, non-trivial work (parsing, computing, copying a large
structure) at once, or contexts where writes are extremely rare relative to read volume and
read latency under serialization would itself be a measured problem. The two measurements
here (a single word, and 200 words) were both apparently too cheap to clear that bar on this
hardware; this is exactly why "read-heavy, therefore shared_mutex" is a hypothesis to
measure, not a rule to apply by default.

**"If you did observe writer starvation on some other platform, how would you fix it
without abandoning shared_mutex?"** A common technique: readers check a "writer is waiting"
flag before taking `shared_lock`, and back off (or take the exclusive path instead) if it's
set, giving a waiting writer a chance to get in ahead of a fresh wave of readers. This is
extra bookkeeping you write yourself; the standard's `shared_mutex` doesn't include a
fairness policy, which is exactly why this course's own measurement (no starvation observed)
isn't a portable guarantee, it's telling you about libc++'s current implementation choice,
not the standard's contract.

**"This is used for a config store, what if the writer needs to read the current value
before deciding what to write (a read-modify-write), not just blindly overwrite it?"** That
needs the whole operation under one `unique_lock`, not a `shared_lock` read followed by a
separate `unique_lock` write, between those two calls another writer could run, and the
"decide what to write" step would be acting on stale data. `std::shared_mutex` has no
built-in upgrade path from a shared lock to a unique one (unlike some other languages'
reader-writer locks); you either take the exclusive lock for the whole read-then-write, or
you accept that your read informed a decision that might already be stale by the time you
act on it.
