Split the array into one chunk per thread, and have each thread compute the prefix sum of
just its own chunk, as if that chunk were the whole array. Run that against the tests — it's
correct for chunk 0, and wrong for every chunk after it. What does chunk 0 have that no
other chunk has?

---

Chunk 0 needed no information from outside itself — there was nothing before it. Every other
chunk's *true* starting value isn't zero, it's the sum of everything in every chunk before
it. That "everything before this chunk" total is exactly one number per chunk — small,
independent of how large the array is, and cheap to compute once every thread has reported
its own chunk's total.

---

Three passes: (1) each thread computes its chunk's local prefix sum *and* records its
chunk's total — this part needs no coordination between threads at all; (2) a short,
ordinary sequential loop over just the `num_threads` chunk totals turns them into starting
offsets (chunk `t`'s offset is the sum of chunk totals `0..t-1`) — cheap, because
`num_threads` is small; (3) each thread adds its chunk's offset to the local values it
already computed in pass 1.
