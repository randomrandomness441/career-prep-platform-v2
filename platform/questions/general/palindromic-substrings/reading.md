## 1. Reframe the problem

Instead of checking every one of a string's `O(n²)` substrings individually (expensive,
and easy to get right but slow), flip the question around: every palindrome has exactly
one center, and grows outward symmetrically from it. There are only `2n-1` possible
centers in a string of length `n`, one on each character, and one in each gap between
two adjacent characters. Expand outward from every one of them, and you've found every
palindromic substring in `O(n²)` time without ever generating a substring you don't need.

## 3. The broken version, first

The natural first draft expands from every character:

```cpp
for (int i = 0; i < n; ++i) total += expand(s, i, i);
```

Run it on `"abba"`:

```
count_palindromic_substrings("abba") -> 4 (expected 6)
```

It finds `a`, `b`, `b`, `a`, four single-character palindromes, and stops. It never
finds `"bb"` or `"abba"` itself, because both of those palindromes are centered *between*
two characters, not on one, and the loop only ever calls `expand(s, i, i)`. Adding the
missing `expand(s, i, i+1)` call for every index catches every even-length palindrome the
first version was structurally incapable of finding, not a rare edge case, but half of
all the centers a string actually has.

## 6. Where this solution fails

- **Very long strings with long runs of the same character.** `"aaaa...a"` (n copies)
 has `O(n²)` palindromic substrings, the answer itself grows quadratically, so no
 algorithm can return the count faster than `O(n²)` in the worst case, expand-around-
 center included. It's optimal, not magic.
- **Unicode / multi-byte characters.** This solution treats `s` as a sequence of
 `char`s (bytes). A multi-byte UTF-8 character would be split across several `char`
 positions, and "reads the same forwards and backwards" at the byte level isn't the
 same question as "reads the same forwards and backwards" at the character level for
 non-ASCII text.
