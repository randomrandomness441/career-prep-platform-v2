`++count_` on a plain, shared `long` is a read-modify-write with no exclusion between
threads. Run the boilerplate against the tests before assuming a tight loop of plain
increments is close enough — under real concurrent load it loses a large fraction of its
increments, reliably.

---

The simplest fix is making the counter `std::atomic<long>` and using `fetch_add`. That's
correct, but every thread is still contending on the *same* atomic — every increment from
every thread is a potential cache-line fight with every other thread's increment. Given
`num_shards` is part of the interface, that's a hint the intended answer spreads the
counting out, not just makes one shared counter atomic.

---

Give each thread its own shard to increment — pick one based on something stable per thread
(`std::hash<std::thread::id>{}(std::this_thread::get_id()) % num_shards`, computed fresh
each call, or cached once per thread if you want to avoid recomputing the hash). Different
threads land on different shards most of the time, so most increments touch memory no other
thread is touching. `total()` just sums every shard — cheap, because it's called rarely.
