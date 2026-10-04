# Count Palindromic Substrings

## ELI5: every mirror hiding inside a word

A palindrome is a string that reads the same forwards and backwards, "a", "bb", "aba".
Given a longer string, lots of little palindromes are often hiding inside it as
substrings: "abba" contains "a", "b", "b", "a" (the single letters), "bb", and "abba"
itself, six palindromic substrings total, some of them just one letter long. Your job
is to count every one.

## What you're actually building

```cpp
int count_palindromic_substrings(const std::string& s);
```

Count every contiguous substring of `s` that reads the same forwards and backwards,
including single characters (always palindromes) and the whole string if it qualifies.
Two substrings at different positions with identical text both count separately, this
counts *occurrences*, not distinct palindromic strings.

## Requirements

1. Every single character counts as a length-1 palindrome.
2. Counts both odd-length ("aba") and even-length ("abba", "bb") palindromes.
3. Works for the empty string (answer: 0) and a single character (answer: 1).

## Why the constraints exist

**Every palindrome has a center**, either a single character (odd length) or the gap
between two characters (even length), and expands symmetrically outward from it. A
string of length `n` has `n` single-character centers and `n-1` between-character
centers: `2n-1` centers total, and checking both kinds is what separates a correct
answer from one that quietly only finds half of them.
