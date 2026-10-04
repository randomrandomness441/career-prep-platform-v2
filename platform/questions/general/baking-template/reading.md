## 1. Reframe the problem

"Does this box have exactly what the template asks for" sounds like a set-membership
question, but it's actually a *counting* question wearing a set-question's clothes.
Two strings using the same letters but different quantities of them are not a match.
So the check has to compare frequency tables, not just which letters appear.

## 3. The broken version, first

The natural first draft reaches for `std::set<char>`. It's a very natural instinct,
since "which letters are present" really does sound like set membership.

```cpp
std::set<char> box_set(box.begin(), box.end());
std::set<char> tmpl_set(tmpl.begin(), tmpl.end());
return box_set == tmpl_set;
```

Run it on `"cm"` versus `"ccm"`, the exact case the problem statement calls out as a
*non*-match:

```
same letters, different counts: got 1, expected 0
```

`std::set<char>{'c','m'}` and `std::set<char>{'c','c','m'}` are the same set. A `set`
collapses duplicates, which is exactly the information this problem hinges on. The fix
isn't a different container, it's a different question. Not "which letters appear" but
"how many of each letter appears," which is what a 26-slot frequency array (or a
`std::map<char,int>`) answers and a `std::set` structurally cannot.

## 6. Where this solution fails

- **Non-lowercase-a-through-z input.** The 26-slot array in the solution assumes every
  character is `'a'`-`'z'`. A digit, uppercase letter, or punctuation mark indexes
  outside the array, which is undefined behavior, not a graceful rejection. A version
  taking arbitrary input would use a `std::unordered_map<char,int>` instead, trading a
  little speed for handling any character.
