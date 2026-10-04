# Collatz Sequence Length

## ELI5: a strange rule that (probably) always leads home

Pick any positive number. If it's even, cut it in half. If it's odd, triple it and add
one. Repeat. Weirdly, no matter what number you start with, this always seems to
eventually reach 1. Nobody has ever found a counterexample, and nobody has ever proven
it always happens either. That's the actual, unsolved "Collatz Conjecture." Your job
isn't to prove anything, just count how many steps it takes to get from a given
starting number down to 1.

## What you're actually building

```cpp
long long collatz_steps(long long n);
```

Return the number of steps to reach 1, starting from `n` (`n >= 1`). Reaching 1 in zero
additional steps (i.e., `n == 1`) returns 0.

## Requirements

1. `n = 1` returns `0`.
2. Even `n`: the next value is `n / 2`.
3. Odd `n`: the next value is `3n + 1`.
4. Count every step taken until the value reaches exactly `1`.

## Why the constraints exist

**The sequence can climb well above its starting value before it ever comes down.**
Starting from a large odd `n`, `3n + 1` can be large enough to overflow a 32-bit `int`
even though `n` itself easily fits in one. This is one of the most common real bugs in
an otherwise-correct Collatz implementation, and it's silent: the program keeps running,
just with a wrapped-around, wrong number, and produces a wrong step count instead of a
crash.
