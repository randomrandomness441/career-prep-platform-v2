# Multithreaded Argsort (Few Distinct Values)

## ELI5: sorting a mountain of laundry by color, not by comparing socks one at a time

Imagine a huge pile of laundry, millions of items, but there are only a few hundred
actual colors in the whole pile. You wouldn't sort this the way you'd sort a hand of
playing cards, comparing pairs and swapping. You'd set out a few hundred labeled bins,
one per color, and just toss each item straight into its bin as you pick it up. Reading
the bins off in order gives you the fully sorted pile, and you never once compared two
items directly to each other, you just asked each item "which bin do you belong in?"

Several people can do this at once, tossing items into the same shared set of bins in
parallel, as long as nobody's throw collides with anybody else's mid-air.

## What you're actually building

Sort a large array by returning the *permutation* that sorts it (an argsort, not an
in-place sort), for exactly this shape: millions of elements, but only a few hundred
distinct values.

```cpp
std::vector<std::size_t> parallel_argsort(const std::vector<int>& values, int num_buckets,
int num_threads = 8);
```

`values[i]` is always in `[0, num_buckets)`, that's the "color" of item `i`. Return
`perm` such that `values[perm[0]] <= values[perm[1]] <= ... <= values[perm[n-1]]`, using
multiple threads, this is counting sort, parallelized, not a generic comparison sort.

## Requirements

1. `perm` must be a valid permutation of `[0, n)`, every original index appears exactly
 once.
2. `values[perm[i]]` must be non-decreasing.
3. Correct under real concurrent execution across many threads, not just single-threaded.
4. `n` may be zero, and `num_buckets` may be as small as 1.

## Why the constraints exist

- **This is `O(n)` counting sort, not `O(n log n)` comparison sort**, don't reach for
 `std::sort`. The whole point is exploiting `num_buckets` being small, the way sorting
 by color only works because there are so few colors.
- **Stability (preserving the relative order of equal elements) is not required.** Two
 socks of the same color can land in either order within their bin.
