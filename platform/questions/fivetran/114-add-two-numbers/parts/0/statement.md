Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

You are given two non-empty linked lists representing two non-negative integers. The digits are stored in reverse order, and each node contains a single digit. Add the two numbers and return the sum as a linked list in the same reversed form.

`public ListNode addTwoNumbers(ListNode l1, ListNode l2)`

- Input: l1 = [2,4,3], l2 = [5,6,4]
  Output: [7,0,8]  (342 + 465 = 807)
- Input: l1 = [0], l2 = [0]
  Output: [0]
- Input: l1 = [9,9,9,9,9,9,9], l2 = [9,9,9,9]
  Output: [8,9,9,9,0,0,0,1]  (9999999 + 9999 = 10009998)

Constraints:
- The number of nodes in each linked list is in the range [1, 100].
- 0 <= Node.val <= 9
- The numbers do not contain leading zeros, except the number 0 itself.