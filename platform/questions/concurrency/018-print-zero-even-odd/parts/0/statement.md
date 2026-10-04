# Print Zero Even Odd

## ELI5: three counters, one placeholder

Three friends are counting out loud together, taking strict turns: one of them only ever
says "zero" (a placeholder, said before every real number), and the other two split the
actual numbers between them, one says the even ones, the other says the odd ones. The
result has to sound like: zero, one, zero, two, zero, three, zero, four..., always
placeholder, then a number, placeholder, then a number.

## What you're actually building

Three threads share one `ZeroEvenOdd` object, constructed with a count `n`.

- Thread A calls `zero()`. It prints `0`, `n` times.
- Thread B calls `even()`. It prints `2, 4, 6, ...` up to `n`.
- Thread C calls `odd()`. It prints `1, 3, 5, ...` up to `n`.

```cpp
class ZeroEvenOdd {
public:
    ZeroEvenOdd(int n);
    void zero(std::function<void(int)> printNumber);
    void even(std::function<void(int)> printNumber);
    void odd (std::function<void(int)> printNumber);
};
```

The combined output must be `0 1 0 2 0 3 0 4 0 5 ...` up to `n`, a `0` before every
number. For `n = 5` the only acceptable sequence is `0 1 0 2 0 3 0 4 0 5`.

The three threads are started in an arbitrary order and the scheduler may run them in any
order.

## Why the constraints exist

- **No busy-waiting.** A thread that can't proceed yet has to actually sleep, not spin
 checking "my turn?" in a loop.
- **All three calls must return when the sequence is finished**, nobody's left asleep.
- **Works for `n` odd and `n` even**, don't special-case away one of the two endings.
