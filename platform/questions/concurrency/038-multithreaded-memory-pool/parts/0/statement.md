# Multithreaded Memory Pool

## ELI5: a gym with a fixed number of identical lockers

A gym has exactly, say, 200 identical lockers, built once, when the gym opened, and
never any more added. Members grab a free locker, use it, and return the key when done.
The *same* 200 lockers get reused all day, over and over, by completely different people.
If every single locker is taken and you walk in wanting one, the gym doesn't build you a
brand-new locker on the spot, it just tells you "sorry, all full" immediately, and you
try again later. That's the whole design: a fixed set of identical slots, handed out and
taken back, with no "just make more" fallback.

## What you're actually building

A fixed-block memory pool: pre-allocate room for `BlockCount` blocks of `BlockSize` bytes
each, once, up front. After that, `allocate()`/`deallocate()` must never call
`malloc`/`new` again, the entire point is avoiding the global allocator (and whatever
lock it uses internally) on a hot path.

```cpp
template <std::size_t BlockSize, std::size_t BlockCount>
class MemoryPool {
public:
    MemoryPool();
    void* allocate(); // nullptr if the pool is exhausted
    bool deallocate(void* p); // false if p wasn't a live allocation from this pool
    std::size_t blocks_in_use() const;
};
```

## Requirements

1. `allocate()`/`deallocate()` are called concurrently from many threads. No two live
 allocations may ever alias the same block, no two members holding the same locker at
 once.
2. **A block handed out by `allocate()` may be `deallocate()`d and immediately handed
 back out again**, the pool constantly recycles the same fixed set of addresses.
 Correctness must hold under that recycling, not just on first use of each block.
3. **`deallocate()` on a pointer that isn't currently a live allocation from this pool**
 (a double-free, or a foreign pointer) must be refused, return `false`, and must not
 corrupt the pool's internal bookkeeping. Handing back a key for a locker you never
 actually rented shouldn't break the front desk's records.
4. **Exhaustion policy: fail fast.** `allocate()` returns `nullptr` immediately when the
 pool is empty, no blocking, no growth, no silent fallback to the global heap. "Sorry,
 all full," not "wait here" or "we'll build you one."
5. `blocks_in_use()` must be exact, under heavy concurrent alloc/dealloc traffic, it
 must reflect the true number of blocks currently handed out, not an approximation.

## Why the constraints exist

- **No calls into the global allocator after construction.** That's the entire point of
 the exercise, avoiding `malloc`/`new` (and whatever lock they use internally) on a hot
 path.
- **`BlockSize` is at least large enough to store an internal free-list link**, you
 don't need extra per-block metadata beyond what fits in the block itself while it's
 free.
