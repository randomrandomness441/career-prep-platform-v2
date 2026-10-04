For each of the `2n-1` centers (n single-character, n-1 between-character), expand
outward with two pointers while the characters on both sides match, counting one
palindrome per successful expansion step.
---
```cpp
int expand(const std::string& s, int lo, int hi) {
    int count = 0;
    while (lo >= 0 && hi < (int)s.size() && s[lo] == s[hi]) {
        ++count; --lo; ++hi;
    }
    return count;
}
```
Call `expand(s, i, i)` for every index `i` (odd-length centers) AND
`expand(s, i, i+1)` for every index `i` (even-length centers), summing both.
