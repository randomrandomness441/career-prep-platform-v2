Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

Design a max stack. It supports the normal stack operations and it also tracks the maximum element. `popMax` removes the maximum, and if the maximum appears more than once it removes the occurrence closest to the top.

Implement these methods:

```java
class Solution {
    public Solution()      // constructs an empty max stack
    public void push(int x) // pushes x onto the stack
    public int pop()        // removes and returns the top element
    public int top()        // returns the top element without removing it
    public int peekMax()    // returns the maximum without removing it
    public int popMax()     // removes and returns the topmost occurrence of the maximum
}
```

Example 1:
Input: push(5), push(1), push(5), top(), popMax(), top(), peekMax(), pop(), top()
Output: [null, null, null, 5, 5, 1, 5, 1, 5]
Explanation: the stack is [5, 1, 5] bottom to top. `popMax` removes the 5 on top, so the stack becomes [5, 1]. The later `pop` returns 1 and the final `top` is 5.

Example 2:
Input: push(-10), push(-20), peekMax(), pop(), peekMax()
Output: [-10, -20, -10]

Example 3:
Input: push(2), push(2), push(2), push(1), popMax(), top()
Output: [2, 1]
Explanation: the stack is [2, 2, 2, 1] bottom to top. `popMax` removes the 2 directly under the 1, so the stack becomes [2, 2, 1].

Constraints:
- -10^7 <= x <= 10^7
- At most 10^5 calls total to push, pop, top, peekMax, and popMax.
- pop, top, peekMax, and popMax are only called when the stack has at least one element.
