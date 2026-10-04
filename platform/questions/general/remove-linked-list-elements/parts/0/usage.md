### How it's called

```cpp
// list: 1 -> 2 -> 6 -> 3 -> 4 -> 5 -> 6, remove val=6
ListNode* head = build_list({1,2,6,3,4,5,6});
head = remove_elements(head, 6);
// resulting list: 1 -> 2 -> 3 -> 4 -> 5

// list: 7 -> 7 -> 7 -> 7, remove val=7
head = build_list({7,7,7,7});
head = remove_elements(head, 7);
// head == nullptr
```
