Count each character in `box` into a 26-slot array (or a `std::array<int,26>`), then
subtract each character of `template` from the same array. If every slot ends at
exactly 0, every letter's count matched exactly.
---
```cpp
bool matches(const std::string& box, const std::string& tmpl) {
    if (box.size() != tmpl.size()) return false;
    std::array<int, 26> count{};
    for (char c : box) ++count[c - 'a'];
    for (char c : tmpl) --count[c - 'a'];
    for (int n : count) if (n != 0) return false;
    return true;
}
```
The length check up front is an easy early exit, not strictly required for correctness
(two different-length strings can never balance to all zeros anyway) but avoids the
wasted work.
