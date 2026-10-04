# Matching Box and Template Pairs

## ELI5: does this box hold exactly the ingredients the recipe card calls for?

A bakery has a box of ingredient letters and a recipe template of ingredient letters for
each order. An order "matches" if the box holds *exactly* the letters the template
calls for. Same letters, same counts, just not necessarily in the same order. "cm" in
the box matches a "mc" template, since it's the same two ingredients. "cm" does **not**
match a "ccm" template, even though every letter in "cm" also appears in "ccm". The
template calls for an extra `c` that the box doesn't have.

## What you're actually building

```cpp
int count_matching_pairs(const std::vector<std::pair<std::string,std::string>>& items);
```

Each pair is `(box, template)`. Count how many pairs have the exact same multiset of
characters, same letters, same counts, order irrelevant.

## Requirements

1. `"cm"` and `"mc"` match (same letters, same counts, different order).
2. `"cm"` and `"ccm"` do **not** match. Same *set* of letters (`{c, m}`), but different
 *counts*, one `c` versus two.
3. Different lengths never match (a necessary, not sufficient, condition).

## Why the constraints exist

**Compare character *counts*, not character *sets*.** The tempting shortcut is to
deduplicate each string down to its unique letters and compare those sets, and that's
wrong. It throws away exactly the count information that distinguishes `"cm"` from
`"ccm"`.
