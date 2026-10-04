Create a dummy node whose `next` points at the real head, and do all your walking from
`&dummy`, not from `head`. Every removal — including "the first real node matches" —
becomes "the node after `cur` matches, skip over it," with no special case.
---
```cpp
ListNode dummy(0);
dummy.next = head;
ListNode* cur = &dummy;
while (cur->next) {
    if (cur->next->val == val) {
        ListNode* doomed = cur->next;
        cur->next = doomed->next;
        delete doomed;
    } else {
        cur = cur->next;
    }
}
return dummy.next;
```
Notice `cur` only advances in the `else` branch — after deleting a match, `cur->next`
is already the next node to check, so advancing `cur` too would skip over it (missing
a match immediately after another match).
