### How it's called

```cpp
threadsafe_list list;
list.push_front(0);
list.push_front(1);
list.push_front(2);   // list now reads 2, 1, 0 front to back

std::vector<int> seen;
list.for_each([&](int v){ seen.push_back(v); });
// seen == {2, 1, 0}

std::optional<int> hit = list.find_first_if([](int v){ return v < 2; });
// hit == 1 (first match walking front to back)

list.remove_if([](int v){ return v % 2 == 0; });   // drops 2 and 0
```

Many threads call `push_front`, `for_each`, `find_first_if`, and `remove_if` on the same
list concurrently — each walk holds at most two node locks at once, so different threads
can be at different points in the list simultaneously.
