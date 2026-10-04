Two parallel arrays, both sized `n`, plus a running `size_`:
- `dense[]` — the elements currently in the set, packed at the front, no gaps.
- `sparse[x]` — where `x` currently sits inside `dense[]`, if it's present at all.
---
`insert(x)`: if already present, do nothing. Otherwise append `x` to the end of
`dense[]` (at index `size_`), record `sparse[x] = size_`, bump `size_`.

`contains(x)`: `x` is present only if `sparse[x]` is a valid index *and*
`dense[sparse[x]] == x` — that double-check is what makes stale, uninitialized
`sparse[x]` values harmless instead of a false positive.
---
`remove(x)`: swap the element at `dense[size_-1]` (the last one) into `dense[sparse[x]]`
(the slot being vacated), fix up that moved element's `sparse[]` entry to point at its
new position, then shrink `size_` by one. No shifting, no gaps, O(1).

`clear()`: **do not loop over anything.** Just set `size_ = 0`. Every `sparse[x]` entry
left over from before is now stale, but `contains`'s double-check (`dense[sparse[x]] ==
x`) means a stale entry can never produce a false positive — it either points past the
new, smaller `size_` (nothing to compare against a length check), or points at a slot
that now holds some *other* value.
