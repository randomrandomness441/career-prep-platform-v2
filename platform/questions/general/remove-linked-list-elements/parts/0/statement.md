# Remove All Linked List Elements Equal to a Value

## ELI5: crossing a name off a guest list, even if it's the first name

You're crossing every occurrence of one name off a handwritten guest list, one entry
linked to the next. Easy enough in the middle of the list, cross the name out, point
the entry before it at the entry after it. The part people forget: what if the very
*first* name on the list is the one you're removing? There's no entry "before" it to
repoint, you have to update where the list itself *starts*, which is a different move
than every other removal on the list.

## What you're actually building

```cpp
struct ListNode {
    int val;
    ListNode* next;
    explicit ListNode(int v) : val(v), next(nullptr) {}
};

ListNode* remove_elements(ListNode* head, int val);
```

Remove every node whose `val` equals `val` from the list and return the (possibly new)
head. Nodes not removed keep their original relative order.

## Requirements

1. Every node equal to `val` is removed, including runs of consecutive matches.
2. Correctly handles the value appearing at the head, including when the **entire**
 list consists of nothing but `val`, which must return `nullptr`.
3. An empty list (`head == nullptr`) returns `nullptr`.
4. Removed nodes must not leak, actually `delete` them, not just unlink and forget.

## Why the constraints exist

**A dummy node in front of the real head turns "removing the head" back into an
ordinary case.** Point a throwaway node's `next` at the real head, run the exact same
"look at the node after me, skip over it if it matches" loop starting from the dummy,
and the head case stops being special, it's just node 0's neighbor, like any other.
