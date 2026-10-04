### How it's called

```cpp
bool a = is_valid_square({0,0}, {1,0}, {1,1}, {0,1});   // true -- adjacent order
bool b = is_valid_square({0,0}, {1,1}, {1,0}, {0,1});   // true -- diagonal order, same 4 points
bool c = is_valid_square({0,0}, {0,0}, {0,0}, {0,0});   // false -- no area
bool d = is_valid_square({0,0}, {2,0}, {2,1}, {0,1});   // false -- 2x1 rectangle, not a square
```
