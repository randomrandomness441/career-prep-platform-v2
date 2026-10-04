# Lock-Free Hash Map (Insert-Only)

## ELI5: a community bulletin board, one cork board per topic

A community center has several cork boards, each one dedicated to a topic: lost pets,
garage sales, carpooling, and so on. People walk up and pin new notices whenever they
like, on whichever board matches their topic. Nobody ever *takes down* a notice in this
version. It's a pin-only board. Nobody has to sign a waiting list to walk up and pin
something. Several people can pin to the same board at once, as long as they never end
up double-pinning the exact same notice.

This question is that, deliberately scoped down: a hash map with no locks anywhere, a
fixed number of buckets (cork boards), insert and lookup only. No delete, no resize.

## What you're actually building

```cpp
template <typename K, typename V>
class LockFreeHashMap {
public:
    explicit LockFreeHashMap(std::size_t num_buckets);
    // Returns true if `key` was newly inserted, false if it was already
    // present (existing value untouched).
    bool insert(const K& key, const V& value);
    bool find(const K& key, V* out) const;   // true and *out set if found
};
```

## Requirements

1. **For any key, at most one concurrent `insert` call may ever return `true`.** No key
   is ever stored twice, no matter how many threads race to insert it at once. If two
   people try to pin the exact same notice at the same moment, only one pin actually
   "counts."
2. A key that's genuinely new must still be insertable. `insert` must never block or
   spin forever.
3. `find` must correctly see any insert that happened-before it, under real concurrent
   access.
4. **No mutex, no `std::lock_guard`.** Every bucket's list is a lock-free, CAS-based
   structure.

## Why the constraints exist

**Never call `delete` on a node that another thread might still be traversing.** Since
this map never removes entries, that means never freeing a node that was ever
successfully published, made reachable from a bucket's head. Nobody's allowed to yank a
notice off the board while someone else might be mid-read of it. This exercise
sidesteps that entirely by simply never taking one down.
