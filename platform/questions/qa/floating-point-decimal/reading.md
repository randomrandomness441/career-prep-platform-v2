A fraction terminates in a given base exactly when its denominator (in lowest terms)
only has prime factors that are also factors of the base. In base 10, the prime factors
of 10 are 2 and 5, so `1/2`, `1/4`, `1/5`, `1/8`, `1/10` all terminate cleanly, but
`1/3` (3 isn't a factor of 10) goes on forever: `0.333...`.

Binary floating point uses base 2, whose only prime factor is 2 itself. So a fraction
terminates in binary only if its denominator is a pure power of 2, `1/2`, `1/4`, `1/8`,
`1/1024` are all exact in binary. **`1/10` is not**, because 10 = 2×5, and that stray
factor of 5 has no exact binary representation, the same structural reason `1/3` fails
in decimal.

This isn't a rounding *bug*, it's the same phenomenon as `1/3` in decimal, just for a
different set of numbers. And it bites in completely ordinary-looking code, real,
executed output:

```
0.1 stored as a double, printed to 20 significant digits: 0.10000000000000000555
0.1 + 0.2 == 0.3 ? false
0.1 + 0.2 printed: 0.30000000000000004441
```

`0.1` isn't stored as exactly one tenth, it's stored as the closest `double` can get,
which is off by about `5.5 × 10⁻¹⁸`. That error is normally invisible, but it means
direct `==` comparison between floating-point results computed two different ways is
almost never safe, `0.1 + 0.2` and `0.3` are each the *closest representable value* to
their true mathematical result, and those two closest-representable-values don't happen
to be the same bit pattern.

**The practical takeaways an interviewer is checking for:**
- Never compare floats with `==`; compare `std::abs(a - b) < epsilon` for some
 tolerance appropriate to the computation, or use a fixed-point/integer representation
 (cents instead of dollars, say) when exactness genuinely matters, like money.
- A type with a fixed, arbitrarily large but still finite size (an arbitrary-precision
 decimal type, say) pushes the problem further out but doesn't eliminate it, `1/3`
 still won't terminate no matter how many decimal digits you allow, because the issue
 is which *fractions* terminate in a given base, not how much storage you give the
 representation.
- This is exactly why financial and storage systems dealing with exact quantities
 (dollars and cents, block counts, byte offsets) use integers or fixed-point decimal
 types, never plain `float`/`double`, for values where "off by 5.5 × 10⁻¹⁸" compounding
 across millions of operations is not an acceptable answer.
