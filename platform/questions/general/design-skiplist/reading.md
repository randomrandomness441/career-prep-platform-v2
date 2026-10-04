## 1. Reframe the problem

A skiplist is a sorted linked list with express lanes stacked on top of it. Every node
lives at level 0 (the full, exact list); a coin-flip decides whether it also gets
promoted to level 1, then another flip decides level 2, and so on, so on average, a
constant fraction of nodes exist at each higher level, giving you `O(log n)` expected
search depth without any of the explicit rebalancing a tree needs. The subtlety that
makes this exercise worth doing is that a node promoted to level 3 is really *four
separate list memberships at once* (levels 0 through 3), and removing it means undoing
all four, not just one.

## 3. The broken version, first

The natural first draft for `erase` finds the node at level 0 and unlinks it there:

```cpp
Node* victim = cur->forward[0];
if (!victim || victim->val != num) return false;
cur->forward[0] = victim->forward[0];
delete victim;
return true;
```

Run the test suite against it, 300 insertions (virtually guaranteeing several nodes
get promoted above level 0 with this course's fixed RNG seed), erase every one of them,
then keep using the structure:

```
exit=139
search, add, erase, duplicates and multi-level cleanup all correct
exit=0
exit=139
exit=139
exit=139
```

Five runs of the exact same program, same fixed seed, no threads anywhere, and it
segfaults 4 times out of 5. That's the signature of undefined behavior, not a logic bug
with a clean, reproducible wrong answer: `delete victim` frees the node, but any level
above 0 that node was promoted to still has a pointer sitting there, unrewritten. The
*next* `add()` or `search()` that descends through one of those higher levels reads
`victim->forward[lvl]`, a field inside memory that's already been freed, and possibly
already reused by the next `new` call for something else entirely. What that read
returns depends on the allocator's mood, which is why the exit code isn't the same
every time even though everything else about the program is deterministic.

## 6. Where this solution fails

- **Very unlucky level distributions.** `kMaxLevel` caps promotion at 16 levels; with
 `p = 0.5` per promotion, reaching level 16 needs 16 consecutive coin flips to land
 the same way, astronomically unlikely for any real workload, but worth knowing the
 cap exists and is a deliberate memory/time tradeoff, not a hidden correctness limit.
- **No rebalancing after mass deletion.** If the structure grows to a large `level_`
 and then almost everything is erased, `level_` never shrinks back down, subsequent
 searches still walk through mostly-empty higher levels before reaching useful ones.
 Harmless for correctness, a minor cost for a workload that grows huge and then drains.
