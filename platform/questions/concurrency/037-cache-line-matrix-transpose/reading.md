## 1. Reframe the problem

Transposing a matrix in parallel looks embarrassingly parallel, every output element is
computed from exactly one input element, no thread ever needs to know what any other thread
is doing. That's true for *correctness*. It says nothing about what happens when two
threads' write regions happen to share a cache line: even though they never touch the same
*element*, every write to one thread's row can still invalidate the other thread's cached
copy of the line their two rows both partly live in. [[029-false-sharing]] showed this for
independent counters; the new wrinkle here is that a matrix row's width is rarely a multiple
of the cache line size, so row boundaries drift out of cache-line alignment as you move down
the matrix, there's no way to choose thread boundaries that dodge this by luck, the storage
layout itself has to guarantee it.

## 3. The broken version, first

The boilerplate's `Matrix` is 64-byte aligned at the base, with a row stride of exactly
`cols` floats, no padding:

```cpp
float& at(int r, int c) { return data_[r * cols_ + c]; }
```

**Why it looks right:** this is the standard, textbook row-major layout, the same thing
you'd write for a single-threaded matrix, and it's completely correct there. Nothing about
it looks wrong until you specifically ask "where does thread 3's first row start, in bytes,
relative to a 64-byte boundary", a question single-threaded code never has a reason to ask.

Checking it directly (not by timing, by the actual addresses the implementation produced),
a 1000x1000 matrix split across 8 threads:

```
row 125 starts at address 0x53847a120, which is not 64-byte aligned
row 375 starts at address 0x53856e360, which is not 64-byte aligned
row 625 starts at address 0x5386625a0, which is not 64-byte aligned
row 875 starts at address 0x5387567e0, which is not 64-byte aligned
```

Four of the seven internal thread boundaries land mid-cache-line, every run, this is a
deterministic consequence of the row width (4000 bytes) not being a multiple of 64, not a
timing accident, so it shows up on every single execution, not just some.

The fix pads the row stride up to the next multiple of 16 floats (64 bytes):

```cpp
stride_ = round_up(cols, 64 / sizeof(float));
```

Every row now starts at a fresh cache line by construction, the fix doesn't depend on the
specific matrix dimensions or thread count at all, unlike the naive layout, which only
happens to be safe when `cols` is a multiple of 16.

## 6. Where this solution fails

- **Padding costs real memory, proportional to how badly the true row width misses a cache
 line multiple.** A `cols` value of, say, 17 pads all the way up to 32, nearly doubling
 storage for a matrix that's barely over one cache line wide per row. The waste is
 worst for narrow matrices and disappears for matrices whose width was already a multiple
 of 16.
- **Measured, and worth reporting honestly: the actual performance difference in this
 exercise's shape is small, not dramatic.** 2000x2000 floats, 8 threads, 10 repeated
 transposes: padded and unpadded both landed around **0.6-0.8 ms per transpose**, with no
 consistent, clearly-separated gap between them. Compare this to
 [[029-false-sharing]]'s ~36x, the difference is that false sharing's real cost comes from
 *repeatedly* fighting over the same contended cache line; a counter incremented in a tight
 loop touches its line thousands of times, ping-ponging it between cores every time. This
 transpose writes each element exactly **once**, the shared boundary lines are touched a
 small, fixed number of times regardless of matrix size, so the contention cost is real (the
 layout check above proves it exists) but small relative to the bulk of uncontended work.
 **Layout correctness and measured performance impact are two different questions, and this
 is a case where the first is unambiguous while the second is modest**, don't assume every
 false-sharing fix pays for itself the way a hot-loop counter's does; measure the specific
 access pattern.
- **This padding technique specifically fixes row-boundary false sharing for a row-based
 thread split.** A different parallelization strategy (2D tile-based, where each thread
 owns a rectangular block rather than a contiguous row range) has its own, differently-shaped
 boundary problem that this fix doesn't address.

## 7. Interview follow-ups

**"You measured almost no speedup from the padding fix, does that mean the exercise's
requirement (no shared cache lines across threads) doesn't actually matter?"** It means this
*specific* workload (write each element once, move on) doesn't pay much for the false
sharing that's present, but the alignment problem is still real, still deterministic, and
still exactly the kind of correctness-adjacent property that TSan and ordinary correctness
testing can't see at all (it's not a data race, every write is to a genuinely distinct
element; it's purely a performance property of memory layout). A workload that revisits
those same boundary rows repeatedly (an in-place transpose applied many times, or a stencil
computation over the same buffer) would show the cost this measurement didn't.

**"When would this padding technique's cost (the wasted memory) NOT be worth paying?"**
When rows are already close to a cache-line multiple in width (little waste, so it's nearly
free) or when a matrix is transposed exactly once and then read in a totally different
access pattern where the write-time boundary alignment stops mattering. The decision is a
straightforward cost/benefit: padding is cheap when waste is small or the buffer is reused
often; skip it for a narrow matrix touched exactly once.

**"How would you actually detect that a program has a false-sharing problem, given the
performance impact can be this workload-dependent?"** Hardware performance counters, not
just wall-clock timing, `perf stat -e cache-misses,cache-references` on Linux (or
Instruments' equivalent on this machine) shows an unusually high miss rate relative to the
data actually being touched, which is the real, direct signature. Wall-clock timing alone,
as this reading's own measurement shows, can be too noisy or too small a fraction of total
work to reliably reveal a real, still-worth-fixing layout problem.
