# Why Can't All Decimals Be Represented Exactly in Binary Floating Point?

## ELI5: some fractions just don't come out even, no matter the base

In base 10, `1/3` doesn't terminate, you get `0.333...` forever, because 3 isn't a
factor of 10. But `1/2` terminates fine: `0.5`, exactly. Which fractions terminate
depends entirely on which base you're counting in. Binary (base 2) has exactly the same
issue, just with a different set of "bad" fractions: `1/2` is fine in binary too
(`0.1`), but `1/10`, an utterly ordinary decimal number, `0.1`, does **not** terminate
in binary, for the same structural reason `1/3` doesn't terminate in decimal.

## The question

**Why can't all decimal numbers be represented exactly using binary floating point,
and can you name a specific example?**

A fundamentals question reported directly from a Pure Storage interview.
