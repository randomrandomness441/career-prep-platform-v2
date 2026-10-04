### How it's called

```cpp
Skiplist sl;
sl.add(1);
sl.add(2);
sl.add(3);
sl.search(0);   // false
sl.add(4);
sl.search(1);   // true
sl.erase(0);    // false -- 0 was never added
sl.erase(1);    // true
sl.search(1);   // false

sl.add(5);
sl.add(5);       // duplicates are tracked separately
sl.erase(5);      // removes one copy
sl.search(5);     // still true -- one copy remains
```
