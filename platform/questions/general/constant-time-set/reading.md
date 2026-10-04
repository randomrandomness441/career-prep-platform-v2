## 1. Reframe the problem

Membership testing over a fixed, known range `[0, n)` doesn't need a hash table at
all, direct indexing already gives `O(1)` lookup for free. The actual puzzle is
`clear()`: any implementation that resets `n` slots one at a time is `O(n)`, and the
fix isn't a cleverer loop, it's making "cleared" a *fact you can check per-element in
`O(1)`*, instead of a state you have to actively install into every slot.

## 3. The broken version, first

The natural first attempt gets the dense/sparse shape right, `dense_` holds the
elements packed at the front, `sparse_[x]` records where `x` lives inside `dense_`,
but checks membership with only half the necessary test:

```cpp
bool contains(int x) const { return sparse_[x] < size_; }
```

Insert 10 elements into a set sized for 20, then ask about one that was never
inserted:

```
contains(15) before 15 was ever inserted: true
```

`sparse_[15]` was never written, it's whatever a freshly-constructed `std::vector<int>`
zero-initializes it to, which is `0`. And `0 < size_` (10) is true, so the check passes,
even though nothing about slot 15 has anything to do with the value 15. The same false
positive shows up for a value that *was* inserted and later removed: its old
`sparse_[]` entry is left untouched (removal only rewrites the entry for whichever
element got swapped into its vacated slot), so it can still look "within range" long
after it's gone.

The fix is one extra comparison: `sparse_[x] < size_ && dense_[sparse_[x]] == x`. The
second half confirms the slot `sparse_[x]` points at actually holds `x`, turning "this
index happens to be in bounds" into "this index genuinely maps back to the value being
asked about." That's the whole trick this data structure runs on: never erase stale
data, just make every read self-verifying so stale data can't be mistaken for real data.

## 6. Where this solution fails

- **Negative or out-of-range elements.** This implementation trusts `x` is always in
 `[0, n)` and indexes `sparse_[x]` directly, a negative or too-large `x` is undefined
 behavior (out-of-bounds access), not a graceful `false`. A production version taking
 untrusted input would bounds-check `x` before touching either array.
- **`n` fixed at construction.** There's no growth path; a set that needs to hold
 values outside its original `[0, n)` range needs to be rebuilt from scratch at a
 larger `n`, which is an `O(n)` operation itself, fine as an occasional resize, not
 something to do per-insert.
