# Design a Skiplist

## ELI5: an express lane over a regular line

Picture a sorted line of people, and finding someone specific means walking the line one
person at a time, slow for a long line. Now add an "express lane" above it that only
touches every few people, letting you skip ahead in big strides and only drop down to
the regular line once you're close. Add another express lane above *that*, skipping even
further. That stack of lanes, each one a sparser version of the line below it, is a
skiplist: a sorted structure you can search in roughly `O(log n)` by skipping down
through levels, without the strict rebalancing a balanced tree needs.

## What you're actually building

```cpp
class Skiplist {
public:
    Skiplist();
    bool search(int target) const; // is target present?
    void add(int num); // insert num (duplicates allowed)
    bool erase(int num); // remove ONE occurrence of num; false if absent
};
```

## Requirements

1. `search` returns whether `target` is currently present anywhere in the structure.
2. `add` inserts `num`; duplicate values are allowed and each is tracked separately,
 adding the same value twice means `erase`-ing it once should still leave one copy
 findable by `search`.
3. `erase` removes exactly one occurrence of `num` (if any exist) and returns whether
 one was found and removed.
4. Every node that participates in the structure at any level must be reachable and
 consistent, a node erased at the base level must not still be findable by jumping
 in through a higher level that still points at it.

## Why the constraints exist

**A node exists at every level up to some randomly chosen height, not just level 0.**
When you insert a value, you flip a coin (conceptually) to decide how many levels it
appears on, most values stay low, a few get promoted high, and that random spread is
what keeps the whole structure balanced *on average* without ever needing to explicitly
rebalance it. `erase` has to remove the node from *every* level it was promoted to, not
just the bottom one, leaving it dangling at a higher level is what silently breaks
`search`.
