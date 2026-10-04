Your submission is a single class named `Solution`. It must expose `public int jump(int[] nums)`. The harness compiles your file next to a plain `main()` checker, so there is no JUnit and no extra setup.

Worked call site:

```java
Solution s = new Solution();
int jumps = s.jump(new int[]{2, 3, 1, 1, 4});
// jumps == 2
```