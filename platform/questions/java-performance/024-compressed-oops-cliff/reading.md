## 1. Reframe

Heap sizing isn't a smooth dial where more is uniformly safer. There's a real, sharp
boundary built into how the JVM represents object references at all, and crossing it
changes the size of every single object in the heap, not just new ones.

## 2. What was actually measured, for real

Precisely bisected on this machine, JDK 21: `-XX:+UseCompressedOops` is ergonomically
`true` at `-Xmx32730m` and `false` at `-Xmx32740m` — the real cutoff, not the commonly
repeated "32GB" approximation. Separately, forcing the flag explicitly at a *fixed* 2GB
heap (so the comparison is fair and doesn't require actually having 32GB+ of RAM, which
this machine doesn't): 37.7 bytes per object with compression on, 51.2 bytes per object
with it off, for an identical object shape — about 36% bigger.

## 3. The broken version, first

"When in doubt, give it more heap" is a reasonable-sounding default that fails
specifically and sharply here: an engineer chasing memory pressure by increasing heap
size, without knowing this boundary exists, can cross it accidentally and make the
*effective* memory situation worse — more bytes per object means the same live data now
occupies more heap, which can mean *more* frequent GC, not less, even though the nominal
heap size went up.

## 4. Interview follow-ups

- Does `ObjectAlignmentInBytes` (default 8 on this JVM) relate to why 32GB specifically
  is the theoretical boundary? Yes — a 32-bit reference can address 4 billion distinct
  values; if every object is required to start on an 8-byte boundary, that same 32-bit
  reference can address 4 billion × 8 bytes = 32GB of object space, which is exactly
  where the naive theoretical maximum comes from.
- Is there a way to keep compressed oops benefits past that heap size? Increasing
  `-XX:ObjectAlignmentInBytes` beyond 8 (to a larger power of two) extends how much
  address space a 32-bit compressed reference can reach, at the cost of more padding
  waste per small object (since every object now rounds up to a bigger alignment
  boundary) — a real, different tradeoff, not a free extension of the same benefit.
