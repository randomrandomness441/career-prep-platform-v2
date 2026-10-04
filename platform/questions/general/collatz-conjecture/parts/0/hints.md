The loop itself is short:

```cpp
long long steps = 0;
long long cur = n;
while (cur != 1) {
    cur = (cur % 2 == 0) ? cur / 2 : 3 * cur + 1;
    ++steps;
}
return steps;
```

The trap isn't the loop — it's the type of `cur`. If `cur` is declared `int` while `n`
is `long long`, `3 * cur + 1` computes in 32-bit arithmetic and can wrap around long
before `cur` itself looks anywhere near `INT_MAX`. Keep every intermediate value in the
same wide type as `n` throughout.
