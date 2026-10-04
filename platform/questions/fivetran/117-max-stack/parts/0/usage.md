Your submission is a single class named `Solution`. It plays the role of `MaxStack`, so each test constructs a fresh instance and drives it with method calls. No main method and no I/O.

```java
Solution stk = new Solution();
stk.push(5);
stk.push(1);
stk.push(5);
stk.top();     // 5
stk.popMax();  // 5, stack is now [5, 1]
stk.top();     // 1
stk.peekMax(); // 5
stk.pop();     // 1
stk.top();     // 5
```
