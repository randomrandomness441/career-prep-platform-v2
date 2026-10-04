Each node needs a value and a `forward` pointer *per level* it participates in — not
just one `next` pointer. A node promoted to level 3 has forward pointers at levels
0, 1, 2, and 3, each pointing to the next node that also exists at that level.
---
Searching: start at the highest level of a sentinel head node, walk forward while the
next node's value is less than the target, then drop down a level and repeat — this is
the "express lane, then the regular line" descent. Insertion and erasure both need this
same descent first, to find, at every level, the node immediately before where the
target value would be.
---
For `erase`, keep the "node immediately before the target, at each level" pointers from
the descent (an array indexed by level). Once you've found the actual node to remove
(if it exists), unlink it at *every* level it appears at, using those saved
predecessors — not just at level 0. A node promoted to level 3 needs 4 pointer
rewrites (levels 0 through 3) to be fully removed; missing any one of them leaves a
stale forward pointer that `search` can still walk into.
