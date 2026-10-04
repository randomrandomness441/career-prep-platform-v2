# A Constant-Time Set (Sparse Set)

## ELI5: a coat-check with numbered pegs, not a search

A coat-check with numbered pegs doesn't need to *search* for your coat, you hand over
ticket #47, they walk straight to peg 47. That's an array with direct indexing: no
searching, just arriving. Now imagine the coat-check also needs a "clear everything
out" button at the end of the night, and it needs to be just as instant, not "walk
every single peg and check if there's a coat on it," which would take longer the more
pegs there are, however many are actually in use.

## What you're actually building

```cpp
class FastSet {
public:
    explicit FastSet(int n); // elements are integers in [0, n)
    void insert(int x);
    void remove(int x);
    bool contains(int x) const;
    void clear();
    std::vector<int> to_vector() const; // every element currently in the set, any order
};
```

## Requirements

1. Elements are integers in `[0, n)`, fixed at construction.
2. `insert`, `remove`, `contains` and `clear` must all be genuinely **O(1)**, not
 "fast in practice," actually independent of `n` and independent of how many elements
 are currently in the set.
3. `to_vector()` returns exactly the elements currently present, in any order, and
 costs time proportional to the *current size of the set*, not to `n`.
4. Duplicate `insert`s of the same element and `remove`s of an absent element are both
 harmless no-ops.

## Why the constraints exist

**`clear()` genuinely cannot afford to touch every slot.** A "reset an array of `n`
booleans" implementation of `clear()` is `O(n)`, correct, but it silently violates the
whole point of this exercise the moment the set is cleared and refilled repeatedly on a
large `n`. The fix (see the reading) doesn't erase anything on `clear()` at all, it
just declares the existing contents void by resetting a single size counter, and makes
every other operation cheap enough to double-check that a slot's stale leftover data
doesn't get mistaken for a real membership.
