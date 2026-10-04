## This one is different from the rest of the pack

Every question so far handed you data that was already sampled and collapsed for you.
This one doesn't. There's a real, compiling, correct C++ function below. It has a real
performance problem. You will not spot it by reading the code carefully — it reads as
completely reasonable, and that's the point. You have to actually run a profiler on it
yourself and look at the result, the same way you'd do it on a real system.

This is not a hidden-bug trick. The code is correct. It does exactly what it claims to
do. It's also much slower than it needs to be at realistic scale, in a way that's
invisible from the source alone.

## What you're actually building

```cpp
bool seen_before(const std::vector<int>& seen, int id) {
    for (int s : seen) if (s == id) return true;
    return false;
}

std::vector<int> dedupe(const std::vector<int>& ids) {
    std::vector<int> result;
    for (int id : ids) {
        if (!seen_before(result, id)) {
            result.push_back(id);
        }
    }
    return result;
}
```

`dedupe` takes a list of ids and returns them with duplicates removed, keeping the
first occurrence of each id and preserving their original relative order. That's the
whole contract, and the version above already satisfies it.

**Before you touch the code, profile it.** Compile it into a small standalone program
that calls `dedupe` on a large input (a few hundred thousand ids is enough), and point
a real sampling profiler at it while it runs:

- **macOS**: the built-in `sample <pid> <duration>` command, or install `samply`
  (`brew install samply`) and run `samply record ./yourprogram` for an interactive
  flame graph in your browser.
- **Linux**: `perf record -F 999 -g -- ./yourprogram`, then `perf script | stackcollapse-perf.pl
  | flamegraph.pl > out.svg` (see question 003 for this pipeline), or `samply` works
  here too.

Look at where the width actually is. It will not be where a quick guess says it should
be.

## Requirements

1. Fix `dedupe` so it produces the exact same output (same unique ids, same
   first-occurrence order) but scales to large inputs without the cost you found.
2. Submit expects a plain function with this signature: `std::vector<int>
   dedupe(const std::vector<int>& ids);`. Nothing else about the calling convention
   changes.
3. Submitting runs your version against a large input under a real time budget — the
   given version above will not pass it, no matter how many times you retry, because
   the problem isn't in how it's synchronized or ordered, it's in what it does per
   element as the input grows.

## Why this matters

Every real profiling skill you've built in this pack so far was in service of exactly
this moment: a function that looks fine, passes code review, and is slow for a reason
you can only find by looking at where the CPU actually goes.
