Your submission is a single class named `Solution` containing the `addTwoNumbers` method. The `ListNode` helper class already lives in the same package, so you do not need to define or import it.

Call-site example:

```java
Solution sol = new Solution();
ListNode l1 = new ListNode(2, new ListNode(4, new ListNode(3))); // 342
ListNode l2 = new ListNode(5, new ListNode(6, new ListNode(4))); // 465
ListNode result = sol.addTwoNumbers(l1, l2); // 7 -> 0 -> 8, which is 807
```