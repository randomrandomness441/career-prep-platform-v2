### How it's called

```cpp
FastSet s(10);
s.insert(3);
s.insert(7);
s.contains(3);      // true
s.contains(5);       // false
s.remove(3);
s.contains(3);       // false

auto v = s.to_vector();   // {7}

s.clear();
s.to_vector();             // {} -- and this must not cost O(n) to do
s.insert(2);
s.contains(2);              // true
```
