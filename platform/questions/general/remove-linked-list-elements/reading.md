## 1. Reframe the problem

Removing a node from a singly linked list always means the same operation: rewrite the
*previous* node's `next` pointer to skip over it. The head node is the one exception,
it has no previous node, so removing it means changing what `head` itself points to,
which is a structurally different move from every other removal. A dummy node placed
in front of the real head turns that exception back into the ordinary case: now even
the head has a "previous node" (the dummy), and one loop handles everything.

## 3. The broken version, first

The natural first draft handles the head as a special case up front, once, then loops
normally:

```cpp
if (head && head->val == val) { /* remove head */ }
ListNode* cur = head;
while (cur && cur->next) { /* remove cur->next if it matches */ }
```

Run it on a list that's entirely the value being removed, `{7, 7, 7, 7}`:

```
entire list matches (all leading): got size 1, expected size 0
```

The special-case block strips exactly one leading `7`. The new head, the *second*
original node, also equals `val`, but nothing ever checks the head again after that
one-time strip; the main loop only ever inspects `cur->next`, never `cur` itself. A run
of matches longer than one at the front is silently only half-handled.

## 6. Where this solution fails

- **Extremely long lists.** This solution is iterative (no recursion), specifically to
 avoid a `O(n)`-deep call stack, a recursive version of the same logic would
 stack-overflow on a long enough list, which is a real, distinct failure mode from the
 head-handling bug above.
