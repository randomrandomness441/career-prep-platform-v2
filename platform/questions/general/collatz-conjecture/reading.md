## 1. Reframe the problem

The Collatz rule itself is three lines of arithmetic. The actual engineering question
is what *type* those three lines run in. `n` fitting comfortably in a given integer
width says nothing about whether the sequence it generates stays inside that width. A
Collatz sequence can climb well above its starting value (27 climbs to 9232 before it
ever turns around) before eventually coming down to 1.

## 3. The broken version, first

The natural first draft narrows the loop variable to `int`, since `n` "looks small
enough":

```cpp
int cur = static_cast<int>(n);
while (cur != 1) { cur = (cur % 2 == 0) ? cur / 2 : 3 * cur + 1; ++steps; }
```

`n = 715827883` is odd and comfortably inside `int` range, roughly a third of
`INT_MAX`. Run both versions on it:

```
correct (long long throughout): 190 steps
naive   (narrowed to int):      178 steps
```

Different answers, no crash, no warning. `3 * cur + 1` overflows a 32-bit `int`
partway through the sequence and wraps around to some other value entirely. Signed
integer overflow is undefined behavior in C++, but in practice on this platform it
wraps, and the sequence from that point on is simply a different, wrong sequence. The
starting value never looked dangerous. The sequence it produces did, and nothing about
`n` itself would tell you to check.

## 6. Where this solution fails

- **Extremely long-running inputs.** No input in the tested range takes more than a
  few hundred steps, but the Collatz conjecture itself is unproven. There's no known
  upper bound on how many steps a given `n` might take, or even a proof that every `n`
  terminates at all. This implementation has no step limit, so a hypothetical `n` that
  never reaches 1 would loop forever rather than fail gracefully. Every verified `n` so
  far, into the enormous ranges number theorists have brute-force checked, does
  terminate, but "every one checked so far" isn't the same as "proven for all n."
