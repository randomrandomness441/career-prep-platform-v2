# Cache-Line Aware Matrix Transpose

## ELI5: splitting a giant spreadsheet between copyists, one page break per person

You've got a giant spreadsheet and several people copying different chunks of rows out of
it at the same time. If you're not careful about where you split the work, one person's
last row and the next person's first row can end up sharing the same physical page, and
every time either of them scribbles on that shared page, it disturbs the other, the same
"wobbly shared stand" problem from [[029-false-sharing]], just applied to whole rows of a
matrix instead of individual counters. The fix is the same idea too: make sure every
person's section starts on a fresh page, never mid-page.

## What you're actually building

A row-major matrix with its own aligned storage:

```cpp
class Matrix {
public:
    Matrix(int rows, int cols);
    float& at(int r, int c);
    float get(int r, int c) const;
    int rows() const;
    int cols() const;
    std::uintptr_t row_address(int r) const; // instrumentation, see below
};

Matrix parallel_transpose(const Matrix& src, int num_threads = 8);
```

`parallel_transpose` returns a new `Matrix` of shape `(src.cols(), src.rows())` where
`result.get(c, r) == src.get(r, c)` for every `r, c`.

**Required parallelization convention** (so results are predictable): split the output's
rows into `num_threads` contiguous ranges, each of `ceil(out_rows / num_threads)` rows,
in thread order, thread 0 gets rows `[0, chunk)`, thread 1 gets `[chunk, 2*chunk)`, and
so on.

## Requirements

1. Every element of the result must be correct.
2. **No two threads' row ranges may ever land on the same 64-byte cache line.** Storage
 must be laid out so that a boundary between one thread's last row and the next
 thread's first row always falls at the start of a fresh cache line, never in the
 middle of one. Every copyist's section starts on a fresh page.
3. `row_address(r)` (instrumentation, not part of the "real" API) must return the actual
 memory address of row `r`'s first element, so this can be checked directly rather than
 inferred from timing.

## Why the constraints exist

**Storage must still be contiguous and randomly indexable by `(r, c)`**, this isn't
solved by giving each row a separate heap allocation. You need one spreadsheet with
deliberate page breaks, not a stack of index cards.
