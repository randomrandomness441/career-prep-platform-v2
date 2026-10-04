A cache line is 64 bytes. If a row is, say, 1000 floats wide, that's 4000 bytes — not a
multiple of 64. Row 0 starts at some cache-line-aligned address (if the whole buffer is
aligned), but row 1 starts 4000 bytes later, which is 32 bytes past the nearest cache-line
boundary; row 2 starts another 4000 bytes on, drifting further. Nothing about a tightly
packed `row * cols` layout keeps rows aligned to cache lines unless `cols * sizeof(float)`
happens to already be a multiple of 64.

---

Run the boilerplate against the tests before assuming a tightly packed array is fine — some
of the thread-boundary rows don't start at a 64-byte-aligned address, which means the last
few elements of one thread's final row and the first few elements of the next thread's first
row are physically inside the same cache line. Every write from one core invalidates the
other core's cached copy of that whole line, even though the two threads' writes have
nothing logically to do with each other.

---

Pad the **row stride** — not the row's logical width, `cols()`, which callers still index by,
but the actual number of floats between the start of one row and the start of the next in
memory — up to the next multiple of 16 floats (64 bytes). Every row then starts at a fresh
cache line, regardless of how many rows any one thread is given, because a whole number of
padded rows is always a whole number of cache lines. Combine this with allocating the buffer
itself 64-byte aligned (`::operator new(size, std::align_val_t(64))`, or an aligned
allocator) so row 0 itself starts on a line boundary too.
