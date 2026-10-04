# One Fence Instead of Many: std::atomic_thread_fence

## ELI5: a "publish everything in your pocket" rule, instead of one note at a time

Same `Handshake`, same door-and-note scene as [[013-store-buffering]], read that one
first if you haven't. Two threads meet; each announces its own arrival by writing a flag,
then checks whether the other side has already arrived:

```cpp
class Handshake {
public:
    int arriveA(); // returns 1 if it observed that B had already arrived, else 0
    int arriveB(); // returns 1 if it observed that A had already arrived, else 0
};
```

The invariant is the same: it must never be possible for **both** calls to return `0`.

013 fixed this by saying, for each note individually, "pin this one on the door
*immediately*, no pocket delay" (`memory_order_seq_cst` on each flag). This time you're
not allowed to do that per-note, **the flags themselves must stay
`memory_order_relaxed`**, meaning each one, on its own, is still allowed to sit in a
pocket for a moment. Instead, you plant one rule at a specific point in the routine:
"before you're allowed to go check anyone else's door, everything currently in your
pocket must already be pinned up." That single rule is `std::atomic_thread_fence`.

This isn't an arbitrary restriction. In real code you often have *several* related
pieces of state that all need to be published together at one point, a batch of
counters, several fields of a snapshot. Marking every individual write `seq_cst` works,
but scatters the ordering decision across every call site, and every single one has to
be gotten right. A fence lets you make that decision once, in one place, and keep the
individual operations simple and uniform, see reading section 5 for exactly what this
does and doesn't buy you in practice; it's not the free performance win it sounds like.

## Requirements

1. Same invariant as 013: never both `0`.
2. Both `a_` and `b_` (or however you name your two flags) must use
 `memory_order_relaxed` for every `.store()` and `.load()` on them.
3. Establish the necessary ordering using one or more calls to `std::atomic_thread_fence`
 instead.

The boilerplate is 013's original relaxed-only bug, unchanged, no fence at all. Confirm
it still fails the same way before you add one.
