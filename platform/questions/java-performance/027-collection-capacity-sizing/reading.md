## 1. Reframe

Not every real optimization in this pack is a cliff or an order-of-magnitude fix. Some
are small, real, and worth doing at the right scale — knowing the difference between
"dramatic" and "modest but real" is its own skill, separate from finding an effect at
all.

## 3. The broken version, first

A team, having just learned that presizing collections is a real optimization, applies
it everywhere reflexively — every `new ArrayList<>()` in the codebase gets an estimated
capacity argument, including hundreds of tiny, one-off collections that were never going
to resize more than once regardless. The real, measured 7-9% gets applied as effort in
places where it amounts to microseconds saved, while the actual few hot-path
collections that would benefit meaningfully get no more attention than anything else.

## 4. Interview follow-ups

- `HashMap`'s default load factor is 0.75 — why not just default to 1.0 (never resize
  until truly full) to avoid wasted capacity? A higher load factor means more entries
  share the same bucket count, increasing the average chance of hash collisions and
  therefore lookup cost — 0.75 is a deliberate tradeoff between memory efficiency and
  lookup speed, not an arbitrary number, and presizing with a load factor that's too
  aggressive can trade away lookup performance for a memory savings that may not matter.
- Does `ArrayDeque` or `LinkedList` share this same resizing cost story? `ArrayDeque`
  is backed by an array and resizes similarly to `ArrayList` — the same presizing logic
  applies. `LinkedList` doesn't use a backing array at all (each element is its own
  node), so it has no resizing cost in this sense, at the cost of worse cache locality
  and higher per-element memory overhead for everyday access patterns.
